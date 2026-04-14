#include "sprite-atlas.hpp"
#include "sprite-atlas-impl.hpp"
#include "compat/exception.hpp"
#include "src/quads.hpp"
#include <cstdio>
#include <cr/GrowableArray.h>
#include <Corrade/PluginManager/PluginManager.h>
#include <cr/Path.h>
#include <mg/Texture.h>
#include <mg/TextureArray.h>
#include <mg/TextureFormat.h>
#include <mg/Functions.h>
#include <mg/Color.h>
#include <Magnum/GL/Framebuffer.h>
#include <mg/ImageView.h>
#include <Magnum/Image.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <algorithm>

namespace floormat::SpriteAtlas {

uint16_t alloc_more_layers_count(uint16_t cur_layers, const Atlas& A)
{
    // Growth policy: double while VRAM footprint is modest, switch to 1.5x
    // once the atlas crosses 1 GiB
    uint64_t cur_bytes = (uint64_t)cur_layers * (uint32_t)A.layer_size * (uint32_t)A.layer_size * 4;
    constexpr uint64_t gibibyte = 1024*1024*1024;
    if (cur_layers == 0)
    {
        constexpr auto initial_size = 64 * 1024 * 1024;
        const auto layer = A.layer_size * A.layer_size * 4;
        fm_assert(A.layer_size > 0);
        auto num_layers = (initial_size + layer - 1) / layer;
        num_layers = std::max<int>(num_layers, 1);
        fm_assert((int)num_layers == (int)(uint16_t)num_layers);
        return (uint16_t)num_layers;
    }
    else if (cur_bytes >= gibibyte)
    {
        auto new_size = (uint32_t)Math::ceil((float)cur_layers*1.5f) + 1;
        fm_assert(new_size == (uint32_t)(uint16_t)new_size);
        return (uint16_t)new_size;
    }
    else
    {
        fm_debug_assert(cur_layers && (cur_layers & cur_layers-1) == 0);
        cur_layers <<= 1;
        return cur_layers;
    }
}

void realloc_atlas(Atlas& atlas, uint16_t new_n_layers)
{
    fm_assert(atlas.layer_size > 0);
    fm_assert(new_n_layers > atlas.n_layers);

    auto layer_size = (Int)atlas.layer_size;
    auto used_layers = (Int)atlas.layers.size();

    // glTexStorage3D is immutable, so growth is reallocate-and-copy:
    // allocate a fresh Texture2DArray with the bigger layer count, then move
    // the old contents over.
    GL::Texture2DArray new_texture;
    new_texture.setStorage(1, GL::TextureFormat::RGBA8,
                           {layer_size, layer_size, (Int)new_n_layers})
               .setMagnificationFilter(GL::SamplerFilter::Nearest)
               .setMinificationFilter(GL::SamplerFilter::Nearest)
               .setWrapping(GL::SamplerWrapping::ClampToEdge);

    auto rect = Range2Di{{}, {layer_size, layer_size}};
    constexpr auto attachment = GL::Framebuffer::ColorAttachment{0};
    GL::Framebuffer dst_fb{rect};

    // clear the newly added slices to magenta
    for (Int i = used_layers; i < (Int)new_n_layers; i++)
    {
        dst_fb.attachTextureLayer(attachment, new_texture, 0, i);
        dst_fb.clearColor(0, Color4{1.f, 0.f, 1.f, 1.f});
    }

    if (used_layers > 0)
    {
        GL::Framebuffer src_fb{rect};
        for (Int i = 0; i < used_layers; i++)
        {
            src_fb.attachTextureLayer(attachment, atlas.texture, 0, i);
            dst_fb.attachTextureLayer(attachment, new_texture, 0, i);
            GL::Framebuffer::blit(src_fb, dst_fb, rect, GL::FramebufferBlit::Color);
        }
    }

    atlas.texture = move(new_texture);
    atlas.n_layers = new_n_layers;
}

void free_atlas(Atlas& atlas)
{
    // Reset to the same shape Atlas{} would have: texture back to NoCreate
    // (the assignment destroys the old GL object), bookkeeping arrays
    // emptied, n_layers reset so any future alloc_new_shelf rebootstraps
    // via realloc_atlas. layer_size is deliberately preserved — the caller
    // configured it and may want to reuse the same atlas for another batch.
    //
    // arrayClear destructs every element and sets size=0 but keeps the
    // growable capacity intact — cheaper than `= {}` for the reuse case,
    // equivalent when the atlas is about to go out of scope anyway.
    atlas.texture = GL::Texture2DArray{NoCreate};
    atlas.layers = {};
    atlas.height_classes = {};
    atlas.n_layers = 0;
}

Atlas::ShelfPair alloc_new_shelf(Atlas& atlas, uint32_t height)
{
    fm_assert(height > 0 && height <= max_texture_xy);

    // Round the requested height UP to the next multiple of size_class_size
    // (8). All sprites placed in this shelf will be padded to q_height so
    // they share a single height class — keeps per-shelf vertical waste
    // bounded by (size_class_size - 1) regardless of sprite distribution.
    const uint32_t q_height = (height + size_class_size - 1) / size_class_size * size_class_size;
    fm_debug_assert(q_height > 0);
    fm_debug_assert((uint16_t)q_height == q_height);

    // locate the HeightClass for this q_height.
    auto& hcs = atlas.height_classes;
    auto* it = std::lower_bound(hcs.data(), hcs.data() + hcs.size(), q_height,
                                [](const Pointer<HeightClass>& p, uint32_t h) { return p->height < h; });
    auto hc_idx = (uint32_t)std::distance(hcs.data(), it);
    if (hc_idx == hcs.size() || hcs[hc_idx]->height != q_height)
    {
        // No HeightClass exists for this size yet — insert one in place.
        arrayReserve(hcs, 16);
        arrayInsert(hcs, hc_idx, InPlaceInit, InPlaceInit, HeightClass { .height = q_height });
    }
    HeightClass& hc = *hcs[hc_idx];

    // scan existing layers for one with vertical room for q_height.
    auto layer_idx = (uint32_t)-1;
    for (uint32_t i = 0; i < atlas.layers.size(); i++)
        if ((uint32_t)atlas.layer_size - (uint32_t)atlas.layers[i].next_y >= q_height)
        {
            layer_idx = i;
            break;
        }

    if (layer_idx == (uint32_t)-1)
    {
        // No layer has room — must create a new Layer.
        if (atlas.layers.size() >= atlas.n_layers)
        {
            // GL texture has no spare slots either; grow it.
            auto new_n = alloc_more_layers_count(atlas.n_layers, atlas);
            realloc_atlas(atlas, new_n);
        }
        arrayReserve(atlas.layers, 16);
        arrayAppend(atlas.layers, InPlaceInit);
        layer_idx = (uint32_t)(atlas.layers.size() - 1);
    }

    // place the new Shelf at the layer's vertical watermark and advance it.
    Layer& L = atlas.layers[layer_idx];
    arrayReserve(L.shelves, 16);
    Shelf* shelf_ptr = &*arrayAppend(L.shelves, InPlaceInit, InPlaceInit, Shelf {
        .y = L.next_y,
        .height_class = q_height - 1,
        .next_x = 0,
    });
    L.next_y = (uint16_t)(L.next_y + q_height);
    arrayAppend(hc.shelves, shelf_ptr);

    return { shelf_ptr, layer_idx };
}

Sprite* alloc_sprite(Atlas& atlas, uint32_t w, uint32_t h, bool allow_rotate)
{
    fm_assert(w > 0 && h > 0);
    fm_assert(w <= max_texture_xy && h <= max_texture_xy);
    fm_assert(w <= atlas.layer_size && h <= atlas.layer_size);

    // Decide orientation: when rotation is allowed, always take it for now
    // (simplest policy — rotate every eligible sprite). The caller's (w, h)
    // are the ORIGINAL dims; the atlas slot reserves (h, w) when rotated.
    // Callers with pre-rotated pixel data (wall-tileset-tool's top group)
    // must pass allow_rotate=false to preserve their orientation.
    bool is_rotated = allow_rotate;
#if 0
    // DEBUG stress test: force every sprite rotated, overriding
    // allow_rotate=false. All sprites should still display normally —
    // rotation is invisible because texcoords_for_subrect's rotated
    // branch maps caller-space sub_offset/sub_size to the transposed
    // atlas storage correctly, regardless of whether the caller's pixel
    // data was already pre-rotated (wall tops) or not. Exercises the
    // rotation path for ALL registration sites (anim, scenery, wall
    // tops, walls, sides, corners).
    is_rotated = true;
#endif
    const uint32_t slot_w = is_rotated ? h : w;
    const uint32_t slot_h = is_rotated ? w : h;

    // Shelf::height_class stores `q_height - 1` (lets 1024 fit in 10 bits),
    // so we compute the same encoding here for the lookup comparison below.
    const uint32_t q_height_m1 =
        (slot_h + size_class_size - 1) / size_class_size * size_class_size - 1;

    // Step 1: first-fit search over EXISTING shelves of the matching size
    // class. Walking atlas.layers[] directly lets `layer_idx` fall out of
    // the loop index — no need to derive layer ownership separately.
    // Exact-class match (not >=) is intentional: tall shelves never accept
    // shorter sprites, which is what bounds vertical waste at <8px.
    Shelf* shelf = nullptr;
    uint32_t layer_idx = 0;
    for (uint32_t li = 0; li < atlas.layers.size() && !shelf; li++)
        for (auto& sh : atlas.layers[li].shelves)
            if ((uint32_t)sh->height_class == q_height_m1
                && atlas.layer_size - (uint32_t)sh->next_x >= slot_w)
            {
                shelf = &*sh;
                layer_idx = li;
                break;
            }

    // Step 2: nothing fit. Delegate to alloc_new_shelf, which handles the
    // three "construct on demand" branches (HeightClass missing, all layers
    // full, GL storage exhausted) in one place. ShelfPair gives us back the
    // layer index of the freshly-created shelf without a follow-up scan.
    if (!shelf)
    {
        auto [p, i] = alloc_new_shelf(atlas, slot_h);
        shelf = p;
        layer_idx = i;
    }

    // Step 3: reserve horizontal range on the chosen shelf and record the
    // sprite. width/height are stored as size-1 of the ORIGINAL dims
    // (10-bit field represents 1..1024) so `sprite::width()` / `height()`
    // return what the caller passed in — consumers don't see the rotation.
    // Sprite::layer is what render-side code consults to find the slice;
    // it must match the layer the pixels were uploaded to.
    uint32_t x = shelf->next_x;
    shelf->next_x = (uint32_t)(x + slot_w);
    arrayReserve(shelf->sprites, 16);
    auto& sp = arrayAppend(shelf->sprites, InPlaceInit, InPlaceInit, Sprite {
        .x = x,
        .y = shelf->y,
        .layer = layer_idx,
        .width = w - 1,
        .height = h - 1,
        .is_rotated = is_rotated,
    });
    return &*sp;
}

void upload_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels)
{
    // Sprite stores width/height as `size - 1` of the ORIGINAL dims so 1024
    // fits in 10 bits; the +1 here recovers the pixel dimension for the
    // size check.
    const uint32_t orig_w = (uint32_t)sprite.width + 1;
    const uint32_t orig_h = (uint32_t)sprite.height + 1;
    fm_assert((uint32_t)pixels.size().x() == orig_w);
    fm_assert((uint32_t)pixels.size().y() == orig_h);

    if (!sprite.is_rotated)
    {
        // Direct upload
        const ImageView3D view3d{pixels.storage(), pixels.format(),
                                 {pixels.size(), 1}, pixels.data()};
        atlas.texture.setSubImage(0,
            {(Int)sprite.x, (Int)sprite.y, (Int)sprite.layer},
            view3d);
    }
    else
    {
        // Rotate 90° CCW in GL bottom-up memory. Derivation (both views
        // bu-oriented): dst_bu[y_r][x_r] = src_bu[orig_h-1-x_r][y_r].
        // Slot dims are (slot_w=orig_h, slot_h=orig_w).
        const auto px_size = (size_t)pixels.pixelSize();
        const size_t src_row_stride = (size_t)orig_w * px_size;
        const auto* src_base = (const char*)pixels.data();
        const uint32_t slot_w = orig_h;
        const uint32_t slot_h = orig_w;
        Array<char> rotated{NoInit, (size_t)slot_w * (size_t)slot_h * px_size};
        const size_t dst_row_stride = (size_t)slot_w * px_size;
        for (uint32_t y_r = 0; y_r < slot_h; y_r++)
        {
            char* dst_row = rotated.data() + (size_t)y_r * dst_row_stride;
            for (uint32_t x_r = 0; x_r < slot_w; x_r++)
            {
                const char* src_px = src_base
                                   + (size_t)(slot_w - 1 - x_r) * src_row_stride
                                   + (size_t)y_r * px_size;
                std::memcpy(dst_row + (size_t)x_r * px_size, src_px, px_size);
            }
        }
        PixelStorage storage;
        storage.setAlignment(1);
        const ImageView3D view3d{storage, pixels.format(),
                                 {(Int)slot_w, (Int)slot_h, 1},
                                 ArrayView<const void>{rotated.data(), rotated.size()}};
        atlas.texture.setSubImage(0,
            {(Int)sprite.x, (Int)sprite.y, (Int)sprite.layer},
            view3d);
    }
}

std::array<Vector3, 4> texcoords_for_subrect(const Atlas& atlas, const Sprite& sprite,
                                             Vector2ui sub_offset, Vector2ui sub_size,
                                             bool mirror)
{
    const auto full_w = (uint32_t)sprite.width + 1;
    const auto full_h = (uint32_t)sprite.height + 1;
    fm_assert(sub_size.x() > 0 && sub_size.y() > 0);
    fm_assert(sub_offset.x() + sub_size.x() <= full_w);
    fm_assert(sub_offset.y() + sub_size.y() <= full_h);

    const float layer = (float)sprite.layer;
    const float atlas_size = (float)atlas.layer_size;

    float u0, u1, v0, v1;
    if (!sprite.is_rotated)
    {
        const float left      = (float)sprite.x + (float)sub_offset.x();
        const float right     = left + (float)sub_size.x();
        const float top_gl    = (float)sprite.y + (float)full_h - (float)sub_offset.y();
        const float bottom_gl = top_gl - (float)sub_size.y();
        u0 = (left + 0.5f) / atlas_size;
        u1 = (right - 0.5f) / atlas_size;
        v0 = (bottom_gl + 0.5f) / atlas_size; // low v (GL bottom-up)
        v1 = (top_gl - 0.5f) / atlas_size;    // high v
    }
    else
    {
        const float atlas_u_lo = (float)sprite.x + (float)sub_offset.y();
        const float atlas_u_hi = atlas_u_lo + (float)sub_size.y();
        const float atlas_v_lo = (float)sprite.y + (float)sub_offset.x();
        const float atlas_v_hi = atlas_v_lo + (float)sub_size.x();
        u0 = (atlas_u_lo + 0.5f) / atlas_size;
        u1 = (atlas_u_hi - 0.5f) / atlas_size;
        v0 = (atlas_v_lo + 0.5f) / atlas_size;
        v1 = (atlas_v_hi - 0.5f) / atlas_size;
    }

    // Vertex order per Quads::quad convention: {0=BR, 1=TR, 2=BL, 3=TL}.
    Vector2 corners[4] = {
        {u1, v0}, // 0: BR
        {u1, v1}, // 1: TR
        {u0, v0}, // 2: BL
        {u0, v1}, // 3: TL
    };

    // Mirror and rotation permutations over vertex order:
    // - normal:        {0,1,2,3}
    // - rotated only:  {1,3,0,2}  — CCW rotation of vertex→corner mapping
    // - mirrored only: {2,3,0,1}  — horizontal flip (vertex i <-> i XOR 2)
    // - both:          {0,2,1,3}  — mirror applied on top of rotated
    //                               (derived: {1,3,0,2} with pairs swapped
    //                               via XOR 2)
    //
    // Note: the last row differs from Quads::texcoords_at's {3,1,2,0} —
    // that would 180°-rotate the result relative to this setup, which
    // produced visibly upside-down scenery under force-rotate stress test.
    constexpr struct {
        uint8_t a:2, b:2, c:2, d:2;
    } perm[4] = {
        {0, 1, 2, 3},
        {1, 3, 0, 2},
        {2, 3, 0, 1},
        {0, 2, 1, 3},
    };
    const auto p = perm[(unsigned)mirror << 1 | (unsigned)(sprite.is_rotated != 0)];

    return {{
        {corners[p.a], layer},
        {corners[p.b], layer},
        {corners[p.c], layer},
        {corners[p.d], layer},
    }};
}

std::array<Vector3, 4> texcoords_for_sprite(const Atlas& atlas, const Sprite& sprite, bool mirror)
{
    const auto full_w = (uint32_t)sprite.width + 1;
    const auto full_h = (uint32_t)sprite.height + 1;
    return texcoords_for_subrect(atlas, sprite, {0, 0}, {full_w, full_h}, mirror);
}

void dump_atlas(Atlas& atlas, StringView out_path)
{
    fm_assert(atlas.n_layers > 0);

    // stb's image-write encoders use `int` internally for byte counts, so
    // single files past ~2 GB of raw pixels (≈ 512 MP at RGBA8) overflow.
    // Budget each chunk at 256 MP (≈1 GB RGBA8) — half of the limit,
    // leaving headroom for compression workspace. Layers-per-chunk is
    // derived from layer_size so this stays safe if the atlas size grows.
    // e.g. atlas.png → atlas.000.png, atlas.001.png, ...
    constexpr uint64_t pixels_per_chunk = uint64_t{1024} * 1024 * 128;

    const auto split = Utility::Path::splitExtension(out_path);
    const auto base = split.first();
    const auto ext = split.second();
    const auto ls = (Int)atlas.layer_size;
    const auto n = (Int)atlas.n_layers;
    const Int layers_per_chunk = std::max<Int>(1,
        (Int)(pixels_per_chunk / (uint64_t(ls) * uint64_t(ls))));

    PluginManager::Manager<Trade::AbstractImageConverter> mgr;
    auto conv = mgr.loadAndInstantiate("AnyImageConverter");
    fm_assert(conv);

    const Int n_chunks = (n + layers_per_chunk - 1) / layers_per_chunk;
    for (Int c = 0; c < n_chunks; c++)
    {
        const Int first = c * layers_per_chunk;
        const Int count = std::min<Int>(layers_per_chunk, n - first);

        auto chunk = atlas.texture.subImage(0,
            Range3Di{{0, 0, first}, {ls, ls, first + count}},
            Image3D{PixelFormat::RGBA8Unorm});
        // Tightly-packed 3D storage → re-view as `ls` wide, `count*ls` tall
        // 2D image, same bytes. Vertical stack of `count` layers.
        ImageView2D stitched{chunk.format(),
                             {ls, count * ls},
                             chunk.data()};

        char path_buf[1024];
        const int written = std::snprintf(path_buf, sizeof(path_buf),
            "%.*s.%03d%.*s",
            (int)base.size(), base.data(), c,
            (int)ext.size(), ext.data());
        fm_assert(written > 0 && (size_t)written < sizeof(path_buf));
        auto dst = StringView{path_buf, (size_t)written,
                              StringViewFlag::NullTerminated};
        if (!conv->convertToFile(stitched, dst))
            fm_throw("can't write atlas chunk {} to '{}'"_cf, c, dst);
    }
}

void dump_sprite(Atlas& atlas, const Sprite& sprite, StringView out_path)
{
    // Decode the sprite's stored (x, y, layer, width-1, height-1) back to
    // real pixel extents, then read that single rectangle from one layer of
    // the atlas texture and write it as an image via AnyImageConverter.
    const Int w = (Int)sprite.width + 1;
    const Int h = (Int)sprite.height + 1;
    const Int x = (Int)sprite.x;
    const Int y = (Int)sprite.y;
    const Int layer = (Int)sprite.layer;

    auto img = atlas.texture.subImage(0,
        Range3Di{{x, y, layer}, {x + w, y + h, layer + 1}},
        Image3D{PixelFormat::RGBA8Unorm});

    // The returned Image3D is 1-deep; re-view as plain 2D for the converter.
    ImageView2D view{img.format(), {w, h}, img.data()};

    PluginManager::Manager<Trade::AbstractImageConverter> mgr;
    auto conv = mgr.loadAndInstantiate("AnyImageConverter");
    fm_assert(conv);
    if (!conv->convertToFile(view, out_path))
        fm_throw("can't write sprite dump to '{}'"_cf, out_path);
}


} // namespace floormat::SpriteAtlas

namespace floormat {

sprite::sprite(const SpriteAtlas::Sprite* s) : _s{s} { fm_assert(_s); }

uint32_t sprite::x() const           { return (uint32_t)_s->x; }
uint32_t sprite::y() const           { return (uint32_t)_s->y; }
uint32_t sprite::layer() const       { return (uint32_t)_s->layer; }
uint32_t sprite::width() const       { return (uint32_t)_s->width + 1; }
uint32_t sprite::height() const      { return (uint32_t)_s->height + 1; }
bool     sprite::is_rotated() const  { return _s->is_rotated != 0; }

sprite_atlas::sprite_atlas(uint16_t layer_size)
    : _atlas{InPlaceInit}
{
    fm_assert(layer_size > 0 && layer_size <= SpriteAtlas::max_texture_xy);
    _atlas->layer_size = layer_size;
}

sprite_atlas::~sprite_atlas() noexcept = default;
sprite_atlas::sprite_atlas(sprite_atlas&&) noexcept = default;
sprite_atlas& sprite_atlas::operator=(sprite_atlas&&) noexcept = default;

uint16_t sprite_atlas::layer_size() const { return _atlas->layer_size; }
uint16_t sprite_atlas::n_layers() const   { return _atlas->n_layers; }

sprite sprite_atlas::add(const ImageView2D& pixels, bool allow_rotate)
{
    auto size = pixels.size();
    auto* s = SpriteAtlas::alloc_sprite(*_atlas,
                                        (uint32_t)size.x(),
                                        (uint32_t)size.y(),
                                        allow_rotate);
    SpriteAtlas::upload_sprite(*_atlas, *s, pixels);
    return sprite{s};
}

std::array<Vector3, 4> sprite_atlas::texcoords_for(sprite s, bool mirror) const
{
    return SpriteAtlas::texcoords_for_sprite(*_atlas, *s.raw(), mirror);
}

std::array<Vector3, 4> sprite_atlas::texcoords_for(sprite s,
                                                   Vector2ui sub_offset,
                                                   Vector2ui sub_size,
                                                   bool mirror) const
{
    return SpriteAtlas::texcoords_for_subrect(*_atlas, *s.raw(), sub_offset, sub_size, mirror);
}

GL::AbstractTexture& sprite_atlas::texture()
{
    return _atlas->texture;
}

void sprite_atlas::reset()
{
    SpriteAtlas::free_atlas(*_atlas);
}

void sprite_atlas::dump(StringView out_path)
{
    SpriteAtlas::dump_atlas(*_atlas, out_path);
}

void sprite_atlas::dump_sprite(sprite s, StringView out_path)
{
    SpriteAtlas::dump_sprite(*_atlas, *s.raw(), out_path);
}

} // namespace floormat
