#include "anim-traits.hpp"
#include "atlas-loader-storage.hpp"
#include "anim-cell.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"
#include "loader.hpp"
#include "loader/sprite-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "compat/exception.hpp"
#include <cr/StringView.h>
#include <cr/GrowableArray.h>
#include <cr/StridedArrayView.h>
#include <cr/Pointer.h>
#include <cr/Optional.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>
#include <Magnum/PixelStorage.h>

namespace floormat::loader_detail {

using anim_traits = atlas_loader_traits<anim_atlas>;
StringView anim_traits::loader_name() { return "anim_atlas"_s; }
auto anim_traits::atlas_of(const Cell& x) -> const bptr<Atlas>& { return x.atlas; }
auto anim_traits::atlas_of(Cell& x) -> bptr<Atlas>& { return x.atlas; }
StringView anim_traits::name_of(const Cell& x) { return x.name; }
String& anim_traits::name_of(Cell& x) { return x.name; }

void anim_traits::atlas_list(Storage& s)
{
    fm_debug_assert(s.name_map.empty());
    s.cell_array = {};
    arrayReserve(s.cell_array, 16);
    s.name_map[loader.INVALID] = -1uz;
}

auto anim_traits::make_invalid_atlas(Storage& s) -> Cell
{
    fm_debug_assert(!s.invalid_atlas);

    constexpr auto size = Vector2ui{tile_size_xy/2};
    constexpr auto ground = Vector2i(size/2);

    auto frame = anim_frame {
        .ground = ground,
        .offset = {},
        .size = size,
    };
    auto groups = Array<anim_group>{ValueInit, 1};
    groups[0] = anim_group {
        .name = "n"_s,
        .frames = array({ frame }),
    };
    auto def = anim_def {
        .object_name = loader.INVALID,
        .anim_name = loader.INVALID,
        .groups = move(groups),
        .pixel_size = size,
        .scale = anim_scale::fixed{size.x(), true},
        .nframes = 1,
    };
    auto err_tex = loader.make_error_texture(size);
    auto atlas = bptr<class anim_atlas>{InPlace, loader.INVALID, err_tex, move(def)};

    // Register the invalid atlas's single frame in the shared sprite atlas so
    // render-path find_sprite lookups succeed for any object that ended up
    // using this placeholder (e.g., missing asset with loader_policy::warn).
    // Without this, any such object trips fm_assert(sp) at chunk-scenery.cpp.
    // Same pattern as loader/vobj.cpp — third anim_atlas construction site,
    // NOT routed through make_atlas's registration loop.
    {
        const auto px_size = (size_t)err_tex.pixels().size()[2];
        const auto row_stride = (size_t)size.x() * px_size;
        const auto* src_base = (const char*)err_tex.pixels().data();
        PixelStorage storage;
        storage.setRowLength((Int)size.x());
        storage.setAlignment(1);
        const ImageView2D view{storage, err_tex.format(),
                               {(Int)size.x(), (Int)size.y()},
                               ArrayView<const void>{src_base, (size_t)size.y() * row_stride}};
        auto sp = loader.atlas().add(view);
        loader.register_sprite(*atlas, rotation::N, 0, sp.raw());
    }

    auto info = anim_cell {
        .atlas = atlas,
        .name = loader.INVALID,
    };
    return info;
}

auto anim_traits::make_atlas(StringView name, const Cell&) -> bptr<Atlas>
{
    char buf[fm_FILENAME_MAX];
    auto json_path = loader.make_atlas_path(buf, {}, name, ".json"_s);
    auto anim_info = json_helper::from_json<struct anim_def>(json_path);

    for (anim_group& group : anim_info.groups)
    {
        if (!group.mirror_from.isEmpty())
        {
            const auto *begin = anim_info.groups.data(), *end = begin + anim_info.groups.size();
            const auto* it = std::find_if(begin, end, [&](const anim_group& x) { return x.name == group.mirror_from; });
            if (it == end)
                fm_throw("can't find group '{}' to mirror from '{}'"_cf, group.mirror_from, group.name);
            group.frames = array(ArrayView<const anim_frame>{it->frames.data(), it->frames.size()});
            for (anim_frame& f : group.frames)
                f.ground = Vector2i((Int)f.size[0] - f.ground[0], f.ground[1]);
        }
    }

    auto tex = loader.texture(""_s, name);

    fm_soft_assert(!anim_info.object_name.isEmpty());
    fm_soft_assert(anim_info.pixel_size.product() > 0);
    fm_soft_assert(!anim_info.groups.isEmpty());
    fm_soft_assert(anim_info.nframes > 0);
    fm_soft_assert(anim_info.nframes == 1 || anim_info.fps > 0);
    const auto size = tex.pixels().size();
    const auto width = size[1], height = size[0];
    fm_soft_assert(anim_info.pixel_size[0] == width && anim_info.pixel_size[1] == height);

    auto atlas = bptr<class anim_atlas>{InPlace, name, tex, move(anim_info)};

    {
        const auto& def = atlas->info();
        auto pixels = tex.pixels();
        const auto full_width = (uint32_t)size[1];
        const auto format = tex.format();
        // Use the view's byte strides, not full_width * channel_count — the view
        // may carry alignment padding, row-length adjustments, or Y-flip that would
        // make a width * px_size assumption wrong. size[2] is bytes-per-pixel.
        const auto px_size = (size_t)size[2];
        const auto row_stride = (size_t)pixels.stride()[0];
        const auto* src_base = (const char*)pixels.data();

        // Pass 1: non-mirrored groups — add pixels to atlas, register sprites.
        for (const anim_group& g : def.groups)
        {
            if (!g.mirror_from.isEmpty())
                continue;
            const auto r = (rotation)anim_atlas::rotation_to_index(g.name);
            for (uint32_t fi = 0; fi < g.frames.size(); fi++)
            {
                const anim_frame& f = g.frames[fi];
                const uint32_t fw = (uint32_t)f.size.x();
                const uint32_t fh = (uint32_t)f.size.y();
                if (fw == 0 || fh == 0) continue;
                if (fw > SpriteAtlas::max_texture_xy || fh > SpriteAtlas::max_texture_xy)
                    continue;
                // Manually pack the sub-rect into a tight buffer.
                // Source memory is Y-inverted relative to JSON offsets.
                const auto full_height = (uint32_t)size[0];
                const uint32_t mem_y_start = full_height - (uint32_t)f.offset.y() - fh;
                Array<char> packed{NoInit, (size_t)fw * (size_t)fh * px_size};
                for (uint32_t yy = 0; yy < fh; yy++)
                {
                    const auto* src_row = src_base
                                        + ((size_t)mem_y_start + yy) * row_stride
                                        + (size_t)f.offset.x() * px_size;
                    auto* dst_row = packed.data() + (size_t)yy * (size_t)fw * px_size;
                    std::memcpy(dst_row, src_row, (size_t)fw * px_size);
                }
                const ImageView2D view{format,
                                       {(Int)fw, (Int)fh},
                                       ArrayView<const void>{packed.data(), packed.size()}};
                auto sp = loader.atlas().add(view);
                loader.register_sprite(*atlas, r, fi, sp.raw());
            }
        }

        // Pass 2: mirrored groups — alias each frame to the source rotation's
        // registry entry.
        for (const anim_group& g : def.groups)
        {
            if (g.mirror_from.isEmpty())
                continue;
            const auto r = (rotation)anim_atlas::rotation_to_index(g.name);
            const auto source_r = (rotation)anim_atlas::rotation_to_index(g.mirror_from);
            for (uint32_t fi = 0; fi < g.frames.size(); fi++)
            {
                const auto* s = loader.find_sprite(*atlas, source_r, fi);
                if (!s) continue;   // source frame was skipped (oversize) — skip mirror too
                loader.register_sprite(*atlas, r, fi, s);
            }
        }
    }

    return atlas;
}

auto anim_traits::make_cell(StringView name) -> Optional<Cell>
{
    return { InPlace, Cell { .atlas = {}, .name = name } };
}

} // namespace floormat::loader_detail
