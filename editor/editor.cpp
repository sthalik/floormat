#include "editor.hpp"
#include "ground-editor.hpp"
#include "wall-editor.hpp"
#include "scenery-editor.hpp"
#include "vobj-editor.hpp"
#include "compat/hash-table-load-factor.hpp"
#include "src/world.hpp"
#include "src/sprite-atlas.hpp"
#include "loader/loader.hpp"
#include <gtl/phmap.hpp>
#include <cr/GrowableArray.h>
#include <cr/StructuredBindings.h>
#include <mg/Context.h>
#include <mg/Extensions.h>
#include <mg/Range.h>
#include <mg/Texture.h>
#include <mg/TextureFormat.h>
#include <mg/TextureArray.h>
#include <mg/PixelFormat.h>
#include <mg/ImageView.h>
#include <mg/Image.h>

namespace floormat {

struct editor::imgui_palette_cache
{
    gtl::flat_hash_map<const SpriteAtlas::Sprite*, GL::Texture2D> sprite_copies;
    Array<GL::Texture2D> layer_views, retired_views;
    UnsignedInt atlas_id = 0;
    bool clear_requested = false;
};

void editor::on_release()
{
    _last_pos = NullOpt;
}

void editor::clear_selection()
{
    if (auto* ed = current_ground_editor())
        ed->clear_selection();
    else if (auto* wa = current_wall_editor())
        wa->clear_selection();
    else if (auto* ed = current_scenery_editor())
        ed->clear_selection();
    else if (auto* vo = current_vobj_editor())
        vo->clear_selection();
}

auto editor::get_snap_value(snap_mode snap, int mods) const -> snap_mode
{
    if (const auto* mode = current_ground_editor())
        return mode->check_snap(mods);
    else if (const auto* mode = current_wall_editor())
        return mode->check_snap(mods);
    else if (snap != snap_mode::none)
        return snap;
    else
        return snap_mode::none;
}

global_coords editor::apply_snap(global_coords pos, global_coords last, snap_mode snap) noexcept
{
    switch (snap)
    {
    default:
        return pos;
    case snap_mode::horizontal:
        return { pos.raw().x, last.raw().y, nullptr };
    case snap_mode::vertical:
        return { last.raw().x, pos.raw().y, nullptr };
    }
}

void editor::on_mouse_move(world& world, global_coords& pos, int mods)
{
    if (current_ground_editor() || current_wall_editor())
    {
        if (_last_pos && _last_pos->btn != button::none)
        {
            // on_click_() can reset _last_pos.
            const auto last = *_last_pos;
            const Vector2i offset = pos - last.coord;
            const snap_mode snap = get_snap_value(last.snap, mods);
            const global_coords draw_coord = apply_snap(last.draw_coord + offset, last.draw_coord, snap);
            if (draw_coord != last.draw_coord)
            {
                const auto draw_offset = draw_coord - last.draw_coord;
                if (!!draw_offset[0] ^ !!draw_offset[1] && std::abs(draw_offset.sum()) > 1)
                {
                    const auto drawc = draw_coord.raw();
                    auto lastc = last.draw_coord.raw();
                    const auto [minx, maxx] = Math::minmax(drawc.x, lastc.x);
                    const auto [miny, maxy] = Math::minmax(drawc.y, lastc.y);
                    if (draw_offset[0])
                        for (u32 i = minx; i <= maxx; i++)
                            on_click_(world, { i, lastc.y, nullptr }, last.btn);
                    else
                        for (u32 j = miny; j <= maxy; j++)
                            on_click_(world, { lastc.x, j, nullptr }, last.btn);
                }
                else
                    on_click_(world, draw_coord, last.btn);
                _last_pos = { InPlaceInit, pos, draw_coord, snap, last.btn };
            }
            pos = draw_coord;
        }
    }
    else
        on_release();
}

Optional<global_coords> editor::mouse_drag_pos()
{
    if (_last_pos)
        return _last_pos->draw_coord;
    else
        return NullOpt;
}

bool editor::is_dragging() const noexcept { return _last_pos && _last_pos->btn != editor_button::none; }

void editor::on_click_(world& world, global_coords pos, button b)
{
    if (auto* mode = current_ground_editor())
    {
        if (auto opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            case button::place: return mode->place_tile(world, pos, opt);
            case button::remove: return mode->place_tile(world, pos, {});
            default: break;
            }
        }
    }
    else if (auto* mode = current_wall_editor())
    {
        if (auto opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            case button::place: return mode->place_wall(world, pos, opt);
            case button::remove: return mode->place_wall(world, pos, {});
            default: break;
            }
        }
    }
    else if (auto* mode = current_scenery_editor())
    {
        if (const auto& opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            default: break;
            case button::place:
                if (auto& ch = world[pos.chunk3()]; ch.can_place_object(opt.proto, pos.local()))
                    mode->place_scenery(world, pos, opt, *_app);
                break;
            case button::remove:
                mode->place_scenery(world, pos, {}, *_app);
                break;
            }
        }
    }
    else if (auto* mode = current_vobj_editor())
    {
        if (const auto* opt = mode->get_selected(); opt || b == button::remove)
        {
            switch (b)
            {
            default: break;
            case button::place:
                mode->place_vobj(world, pos, opt, *_app);
                break;
            case button::remove:
                mode->place_vobj(world, pos, {}, *_app);
                break;
            }
        }
    }
    on_release();
}

void editor::on_click(world& world, global_coords pos, int mods, button b)
{
    if (auto* mode = current_ground_editor())
    {
        _last_pos = { InPlaceInit, pos, pos, mode->check_snap(mods), b };
        on_click_(world, pos, b);
    }
    else if (auto* mode = current_wall_editor())
    {
        _last_pos = { InPlaceInit, pos, pos, mode->check_snap(mods), b };
        on_click_(world, pos, b);
    }
    else if (current_scenery_editor() || current_vobj_editor())
    {
        _last_pos = {};
        on_click_(world, pos, b);
    }
}

editor::editor(app* a) : _app{a} {}
editor::editor(editor&&) noexcept = default;
editor& editor::operator=(editor&&) noexcept = default;
editor::~editor() noexcept = default;

editor_mode editor::mode() const noexcept { return _mode; }

void editor::set_mode(editor_mode mode)
{
    _mode = mode;
    _imgui_palette_cache->clear_requested = true;
    on_release();
}

void editor::clear_imgui_palette_cache()
{
    auto& c = *_imgui_palette_cache;
    arrayClear(c.retired_views);
    if (c.clear_requested || !c.layer_views.isEmpty() && loader.atlas().texture().id() != c.atlas_id)
    {
        c.sprite_copies.clear();
        arrayClear(c.layer_views);
        c.clear_requested = false;
    }
}

palette_texture_ref editor::palette_texture(sprite s)
{
    auto& c = *_imgui_palette_cache;
    if (GL::Context::current().isExtensionSupported<GL::Extensions::ARB::texture_view>())
    {
        auto& atlas = loader.atlas();
        auto& tex = static_cast<GL::Texture2DArray&>(atlas.texture());
        if (tex.id() != c.atlas_id) [[unlikely]]
        {
            arrayReserve(c.retired_views, Math::max(c.retired_views.size() + c.layer_views.size(), 16uz));
            // This frame's draw commands still name the old views.
            for (auto& v : c.layer_views)
                if (v.id())
                    arrayAppend(c.retired_views, move(v));
            arrayClear(c.layer_views);
            c.atlas_id = tex.id();
        }
        if (s.layer() >= c.layer_views.size()) [[unlikely]]
            arrayResize(c.layer_views, DirectInit, atlas.layer_count(), NoCreate);
        auto& view = c.layer_views[s.layer()];
        if (!view.id()) [[unlikely]]
        {
            view = GL::Texture2D::view(tex, GL::TextureFormat::RGBA8, 0, 1, (Int)s.layer());
            view.setMinificationFilter(GL::SamplerFilter::Nearest)
                .setMagnificationFilter(GL::SamplerFilter::Nearest)
                .setWrapping(GL::SamplerWrapping::ClampToEdge);
        }
        const auto tc = atlas.texcoords_for(s, false);
        return { view.id(), {{ tc[3].xy(), tc[1].xy(), tc[0].xy(), tc[2].xy() }} };
    }

    auto& copies = c.sprite_copies;
    auto it = copies.find(s.raw());
    if (it == copies.end())
    {
        Hash::set_open_addressing_load_factor(copies, copies.size()+1);

        // sprite::width/height always report the ORIGINAL dims. When the
        // packer stored the sprite rotated, the atlas slot is transposed
        // (slot_w = orig_h, slot_h = orig_w); the read region must match
        // the slot, and we un-rotate before upload.
        const u32 orig_w = s.width(), orig_h = s.height();
        const bool rotated = s.is_rotated();
        const u32 slot_w = rotated ? orig_h : orig_w;
        const u32 slot_h = rotated ? orig_w : orig_h;

        const Vector2i dst_size{(int)orig_w, (int)orig_h};
        GL::Texture2D dst;
        dst.setWrapping(GL::SamplerWrapping::ClampToEdge)
           .setMagnificationFilter(GL::SamplerFilter::Nearest)
           .setMinificationFilter(GL::SamplerFilter::Nearest)
           .setStorage(1, GL::TextureFormat::RGBA8, dst_size);

        auto& src = static_cast<GL::Texture2DArray&>(loader.atlas().texture());
        auto img = src.subImage(0,
            Range3Di::fromSize({(int)s.x(), (int)s.y(), (int)s.layer()}, {(int)slot_w, (int)slot_h, 1}),
            Image3D{PixelFormat::RGBA8Unorm});

        if (!rotated) [[likely]]
        {
            const ImageView2D view{img.format(), dst_size, img.data()};
            dst.setSubImage(0, {}, view);
        }
        else
        {
            // Invert upload_sprite's 90° CCW (loader/sprite-atlas.cpp:303):
            //   src[y_o][x_o] = slot[x_o][orig_h - 1 - y_o]
            // Both sides are GL bottom-up. RGBA8 → 4 bytes/px.
            Array<u32> unrotated{NoInit, (usize)orig_w * (usize)orig_h};
            const auto* slot = reinterpret_cast<const u32*>(img.data().data());
            for (u32 y_o = 0; y_o < orig_h; y_o++)
            {
                u32* dst_row = unrotated.data() + (usize)y_o * (usize)orig_w;
                const u32 slot_col = orig_h - 1 - y_o;
                for (u32 x_o = 0; x_o < orig_w; x_o++)
                    dst_row[x_o] = slot[(usize)x_o * slot_w + slot_col];
            }
            const ImageView2D view{img.format(), dst_size, {unrotated.data(), unrotated.size()}};
            dst.setSubImage(0, {}, view);
        }

        it = copies.insert({s.raw(), move(dst)}).first;
    }
    return { it->second.id(), {{ {0, 1}, {1, 1}, {1, 0}, {0, 0} }} };
}

const ground_editor* editor::current_ground_editor() const noexcept
{
    switch (_mode)
    {
    case editor_mode::floor:
        return &*_floor;
    default:
        return nullptr;
    }
}

const wall_editor* editor::current_wall_editor() const noexcept
{
    switch (_mode)
    {
    case editor_mode::walls:
        return &*_wall;
    default:
        return nullptr;
    }
}

const scenery_editor* editor::current_scenery_editor() const noexcept
{
    if (_mode == editor_mode::scenery)
        return &*_scenery;
    else
        return nullptr;
}

const vobj_editor* editor::current_vobj_editor() const noexcept
{
    if (_mode == editor_mode::vobj)
        return &*_vobj;
    else
        return nullptr;
}

ground_editor* editor::current_ground_editor() noexcept
{
    return const_cast<ground_editor*>(static_cast<const editor&>(*this).current_ground_editor());
}

wall_editor* editor::current_wall_editor() noexcept
{
    return const_cast<wall_editor*>(static_cast<const editor&>(*this).current_wall_editor());
}

scenery_editor* editor::current_scenery_editor() noexcept
{
    return const_cast<scenery_editor*>(static_cast<const editor&>(*this).current_scenery_editor());
}

vobj_editor* editor::current_vobj_editor() noexcept
{
    return const_cast<vobj_editor*>(static_cast<const editor&>(*this).current_vobj_editor());
}

} // namespace floormat
