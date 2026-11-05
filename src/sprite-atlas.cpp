#include "sprite-atlas.hpp"
#include "sprite-atlas-impl.hpp"
#include "sprite-constants.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "src/quads.hpp"
#include "compat/fwd/algorithm/lower_bound.hpp"
#include <cmath>
#include <cstdio>
#include <bit>
#include <cr/GrowableArray.h>
#include <cr/Path.h>
#include <cr/StridedArrayView.h>
#include <cr/StringView.h>
#include <cr/StructuredBindings.h>
#include <mg/Texture.h>
#include <mg/TextureArray.h>
#include <mg/TextureFormat.h>
#include <mg/Color.h>
#include <mg/Context.h>
#include <mg/Framebuffer.h>
#include <mg/ImageView.h>
#include <mg/Image.h>
#include <mg/PixelFormat.h>
#include <mg/AbstractImageConverter.h>

namespace floormat::SpriteAtlas {

constexpr u32 size_class_divisor = 16; // height classes per octave = divisor/2 (recommended: 16)

u32 quantize_height(u32 h)
{
    constexpr u32 min_height_class = 4;
    fm_debug_assert(h > 0);
    u32 bucket = Math::max(min_height_class, std::bit_ceil(h) / size_class_divisor);
    return (h + bucket - 1) / bucket * bucket;
}

u32 max_2d_texture_size()
{
    auto size = GL::Texture2D::maxSize();
    fm_assert(size.x() == size.y() && size.x() > 0);
    return (u32)size.x();
}

u16 get_next_layer_count(u16 layer_count, const Atlas& A)
{
    // Growth policy: double while VRAM footprint is modest, switch to 1.5x
    // once the atlas crosses 1 GiB
    u64 cur_bytes = (u64)layer_count * (u32)A.layer_width * (u32)A.layer_height * 4;
    constexpr u64 gibibyte = 1024*1024*1024;
    if (layer_count == 0)
    {
#if 1
        return 1;
#else
        constexpr u32 initial_size = 64 * 1024 * 1024;
        const u32 layer = (u32)A.layer_width * A.layer_height * 4;
        fm_assert(A.layer_width > 0 && A.layer_height > 0);
        u32 num_layers = (initial_size + layer - 1) / layer;
        num_layers = Math::max<u32>(num_layers, 1);
        num_layers = std::bit_ceil(num_layers);
        fm_assert((int)num_layers == (int)(u16)num_layers);
        return (u16)num_layers;
#endif
    }
    else if (cur_bytes >= gibibyte)
    {
        auto new_size = (u32)std::ceil((float)layer_count*1.5f) + 1;
        fm_assert(new_size == (u32)(u16)new_size);
        return (u16)new_size;
    }
    else
    {
        fm_debug_assert(layer_count && (layer_count & layer_count-1) == 0);
        auto x = (u32)layer_count * 2;
        fm_assert(x == (u16)x);
        return (u16)x;
    }
}

void grow_texture(Atlas& atlas, u16 new_layer_count)
{
    fm_assert(atlas.layer_width > 0 && atlas.layer_height > 0);
    fm_assert(new_layer_count > atlas.texture_layer_count && new_layer_count >= atlas.layers.size());

    const auto layer_size = Vector2i{(Int)atlas.layer_width, (Int)atlas.layer_height};
    // layers packed since the last upload exist only on the CPU side
    auto copied_layer_count = Math::min((Int)atlas.layers.size(), (Int)atlas.texture_layer_count);

    // glTexStorage3D is immutable, so growth is reallocate-and-copy:
    // allocate a fresh Texture2DArray with the bigger layer count, then move
    // the old contents over.
    GL::Texture2DArray new_texture;
    new_texture.setStorage(1, GL::TextureFormat::RGBA8,
                           {layer_size, (Int)new_layer_count})
               .setMagnificationFilter(GL::SamplerFilter::Nearest)
               .setMinificationFilter(GL::SamplerFilter::Linear)
               .setWrapping(GL::SamplerWrapping::ClampToEdge);

    auto rect = Range2Di{{}, layer_size};
    constexpr auto attachment = GL::Framebuffer::ColorAttachment{0};
    GL::Framebuffer dst_fb{rect};

    // clear the newly added slices to magenta
    for (Int i = copied_layer_count; i < (Int)new_layer_count; i++)
    {
        dst_fb.attachTextureLayer(attachment, new_texture, 0, i);
        dst_fb.clearColor(0, Color4{1.f, 0.f, 1.f, 1.f});
    }

    if (copied_layer_count > 0)
    {
        GL::Framebuffer src_fb{rect};
        for (Int i = 0; i < copied_layer_count; i++)
        {
            src_fb.attachTextureLayer(attachment, atlas.texture, 0, i);
            dst_fb.attachTextureLayer(attachment, new_texture, 0, i);
            GL::Framebuffer::blit(src_fb, dst_fb, rect, GL::FramebufferBlit::Color);
        }
    }

    atlas.texture = move(new_texture);
    atlas.texture_layer_count = new_layer_count;
}

void ensure_texture(Atlas& atlas)
{
    if (atlas.texture_layer_count >= atlas.layers.size())
        return;
    // Without a context add() sized layers to max_layer_width and max_layer_height, which this GPU must hold.
    const auto max_size = max_2d_texture_size();
    fm_assert(max_size >= atlas.layer_width && max_size >= atlas.layer_height);
    auto layer_count = atlas.texture_layer_count;
    while (layer_count < atlas.layers.size())
        layer_count = get_next_layer_count(layer_count, atlas);
    grow_texture(atlas, layer_count);
}

void free_atlas(Atlas& atlas)
{
    atlas.texture = GL::Texture2DArray{NoCreate};
    atlas.layers = {};
    atlas.height_classes = {};
    atlas.pending = {};
    atlas.staging = {};
    atlas.texture_layer_count = 0;
}

Shelf* alloc_new_shelf(Atlas& atlas, u32 height)
{
    fm_assert(height > 0 && height <= max_sprite_xy);

    const u32 q_height = quantize_height(height);
    fm_debug_assert((u16)q_height == q_height);

    // locate the HeightClass for this q_height.
    auto& hcs = atlas.height_classes;
    auto* it = std::lower_bound(hcs.data(), hcs.data() + hcs.size(), q_height,
                                [](const Pointer<HeightClass>& p, u32 h) { return p->height < h; });
    auto hc_idx = (u32)std::distance(hcs.data(), it);
    if (hc_idx == hcs.size() || hcs[hc_idx]->height != q_height)
    {
        // No HeightClass exists for this size yet — insert one in place.
        arrayReserve(hcs, 16);
        arrayInsert(hcs, hc_idx, InPlaceInit, InPlaceInit, HeightClass { .height = q_height });
    }
    HeightClass& hc = *hcs[hc_idx];

    // scan existing layers for one with vertical room for q_height.
    auto layer_idx = (u32)-1;
    for (u32 i = 0; i < atlas.layers.size(); i++)
        if ((u32)atlas.layer_height - (u32)atlas.layers[i].next_y >= q_height)
        {
            layer_idx = i;
            break;
        }

    if (layer_idx == (u32)-1)
    {
        // No layer has room — must create a new Layer.
        arrayReserve(atlas.layers, 16);
        arrayAppend(atlas.layers, InPlaceInit);
        layer_idx = (u32)(atlas.layers.size() - 1);
    }

    fm_debug_assert(layer_idx < 1u << 14);
    // place the new Shelf at the layer's vertical watermark and advance it.
    Layer& L = atlas.layers[layer_idx];
    arrayReserve(L.shelves, 16);
    Shelf* shelf_ptr = &*arrayAppend(L.shelves, InPlaceInit, InPlaceInit, Shelf {
        .y = L.next_y,
        .next_x = 0,
        .layer = layer_idx,
    });
    L.next_y = (u16)(L.next_y + q_height);
    arrayReserve(hc.shelves, 16);
    arrayAppend(hc.shelves, shelf_ptr);

    return shelf_ptr;
}

static Shelf* find_shelf_with_space(Atlas& atlas, u32 w, u32 h)
{
    const u32 q_height = quantize_height(h);

    auto& hcs = atlas.height_classes;
    auto* it = std::lower_bound(hcs.data(), hcs.data() + hcs.size(), q_height,
                                [](const Pointer<HeightClass>& p, u32 hh) { return p->height < hh; });
    if (it == hcs.data() + hcs.size() || (*it)->height != q_height)
        return nullptr;

    for (auto* sh : (*it)->shelves)
        if (atlas.layer_width - (u32)sh->next_x >= w)
            return sh;

    return nullptr;
}

static Shelf* get_shelf(Atlas& atlas, u32 w, u32 h, bool& is_rotated)
{
    if (auto* shelf = find_shelf_with_space(atlas, w, h))
        return shelf;
    if (auto* shelf = find_shelf_with_space(atlas, h, w))
    {
        is_rotated = !is_rotated;
        return shelf;
    }
    return alloc_new_shelf(atlas, h);
}

Sprite* alloc_sprite(Atlas& atlas, u32 w, u32 h, bool allow_rotate)
{
    fm_assert(w > 0 && h > 0);
    fm_assert(w <= max_sprite_xy && h <= max_sprite_xy);
    fm_assert(Math::max(w, h) <= Math::min<u32>(atlas.layer_width, atlas.layer_height));

    Shelf* shelf;
    bool is_rotated;
#if 1
    if (!allow_rotate) [[unlikely]]
    {
        is_rotated = false;
        shelf = find_shelf_with_space(atlas, w, h);
        if (!shelf)
            shelf = alloc_new_shelf(atlas, h);
    }
    else
    {
        const auto [min, max] = Math::minmax(w, h);
        shelf = get_shelf(atlas, max, min, is_rotated = h > w);
    }
#else
    // debug force-rotating all sprites
    (void)allow_rotate;
    shelf = find_shelf_with_space(atlas, h, w);
    if (!shelf)
        shelf = alloc_new_shelf(atlas, w);
    is_rotated = true;
#endif
    fm_debug_assert(shelf);

    const u32 slot_w = is_rotated ? h : w;
    u32 x = shelf->next_x;
    shelf->next_x = (u32)(x + slot_w);
    arrayReserve(shelf->sprites, 16);
    auto& sp = arrayAppend(shelf->sprites, InPlaceInit, InPlaceInit, Sprite {
        .x = x,
        .y = shelf->y,
        .layer = shelf->layer,
        .width_minus1 = w - 1,
        .height_minus1 = h - 1,
        .is_rotated = is_rotated,
    });
    return &*sp;
}

static Vector2ui slot_size(const Sprite& sprite)
{
    const u32 orig_w = (u32)sprite.width_minus1 + 1;
    const u32 orig_h = (u32)sprite.height_minus1 + 1;
    return sprite.is_rotated ? Vector2ui{orig_h, orig_w} : Vector2ui{orig_w, orig_h};
}

// Writes the sprite's pixels to dst tightly packed, in the slot's orientation.
static void copy_to_slot(char* dst, const Sprite& sprite, const ImageView2D& pixels)
{
    const u32 orig_w = (u32)sprite.width_minus1 + 1;
    const u32 orig_h = (u32)sprite.height_minus1 + 1;
    fm_assert((u32)pixels.size().x() == orig_w);
    fm_assert((u32)pixels.size().y() == orig_h);

    const auto px_size = (usize)pixels.pixelSize();
    const auto src_view = pixels.pixels();
    const usize src_row_stride = (usize)src_view.stride()[0];
    const auto* src_base = (const char*)src_view.data();

    if (!sprite.is_rotated)
    {
        const usize row = (usize)orig_w * px_size;
        for (u32 y = 0; y < orig_h; y++)
            std::memcpy(dst + (usize)y * row, src_base + (usize)y * src_row_stride, row);
        return;
    }

    // Rotate 90° CCW in GL bottom-up memory. Derivation (both views
    // bu-oriented): dst_bu[y_r][x_r] = src_bu[orig_h-1-x_r][y_r].
    // Slot dims are (slot_w=orig_h, slot_h=orig_w).
    const u32 slot_w = orig_h;
    const u32 slot_h = orig_w;
    const usize dst_row_stride = (usize)slot_w * px_size;
    for (u32 y_r = 0; y_r < slot_h; y_r++)
    {
        char* dst_row = dst + (usize)y_r * dst_row_stride;
        for (u32 x_r = 0; x_r < slot_w; x_r++)
        {
            const char* src_px = src_base
                               + (usize)(slot_w - 1 - x_r) * src_row_stride
                               + (usize)y_r * px_size;
            std::memcpy(dst_row + (usize)x_r * px_size, src_px, px_size);
        }
    }
}

static void upload_slot(Atlas& atlas, const Sprite& sprite, PixelFormat format, ArrayView<const char> slot)
{
    const auto size = slot_size(sprite);
    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView3D view3d{storage, format, {(Int)size.x(), (Int)size.y(), 1}, slot};
    atlas.texture.setSubImage(0, {(Int)sprite.x, (Int)sprite.y, (Int)sprite.layer}, view3d);
}

void upload_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels)
{
    ensure_texture(atlas);
    if (!sprite.is_rotated)
    {
        fm_assert(Vector2ui(pixels.size()) == slot_size(sprite));
        const ImageView3D view3d{pixels.storage(), pixels.format(), {pixels.size(), 1}, pixels.data()};
        atlas.texture.setSubImage(0, {(Int)sprite.x, (Int)sprite.y, (Int)sprite.layer}, view3d);
        return;
    }
    const auto size = slot_size(sprite);
    Array<char> slot{NoInit, (usize)size.x() * size.y() * pixels.pixelSize()};
    copy_to_slot(slot.data(), sprite, pixels);
    upload_slot(atlas, sprite, pixels.format(), slot);
}

void stage_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels)
{
    const auto size = slot_size(sprite);
    const auto offset = atlas.staging.size();
    const auto bytes = (usize)size.x() * size.y() * pixels.pixelSize();
    fm_assert(offset + bytes <= UINT32_MAX);
    arrayReserve(atlas.staging, 1 << 20);
    copy_to_slot(arrayAppend(atlas.staging, NoInit, bytes).data(), sprite, pixels);
    arrayReserve(atlas.pending, 64);
    arrayAppend(atlas.pending, PendingSprite{ &sprite, pixels.format(), (u32)offset });
}

void flush(Atlas& atlas)
{
    ensure_texture(atlas);
    for (auto i = 0u; i < (u32)atlas.pending.size(); i++)
    {
        const auto& p = atlas.pending[i];
        const auto end = i + 1 < atlas.pending.size() ? (usize)atlas.pending[i + 1].staging_offset
                                                       : atlas.staging.size();
        upload_slot(atlas, *p.sprite, p.format, atlas.staging.slice(p.staging_offset, end));
    }
    // Keeps the capacity stage_sprite() reserved, so the next add() doesn't allocate it again.
    arrayClear(atlas.pending);
    arrayClear(atlas.staging);
}

std::array<Vector3, 4> texcoords_for_subrect(const Atlas& atlas, const Sprite& sprite,
                                             Vector2ui sub_offset, Vector2ui sub_size,
                                             bool mirror)
{
    const auto full_w = (u32)sprite.width_minus1 + 1;
    const auto full_h = (u32)sprite.height_minus1 + 1;
    fm_assert(sub_size.x() > 0 && sub_size.y() > 0);
    fm_assert(sub_offset.x() + sub_size.x() <= full_w);
    fm_assert(sub_offset.y() + sub_size.y() <= full_h);

    const float layer = (float)sprite.layer;
    const float atlas_w = (float)atlas.layer_width, atlas_h = (float)atlas.layer_height;

    float u0, u1, v0, v1;
    if (!sprite.is_rotated)
    {
        const float left      = (float)sprite.x + (float)sub_offset.x();
        const float right     = left + (float)sub_size.x();
        const float top_gl    = (float)sprite.y + (float)full_h - (float)sub_offset.y();
        const float bottom_gl = top_gl - (float)sub_size.y();
        u0 = (left + 0.5f) / atlas_w;
        u1 = (right - 0.5f) / atlas_w;
        v0 = (bottom_gl + 0.5f) / atlas_h; // low v (GL bottom-up)
        v1 = (top_gl - 0.5f) / atlas_h;    // high v
    }
    else
    {
        const float atlas_u_lo = (float)sprite.x + (float)sub_offset.y();
        const float atlas_u_hi = atlas_u_lo + (float)sub_size.y();
        const float atlas_v_lo = (float)sprite.y + (float)sub_offset.x();
        const float atlas_v_hi = atlas_v_lo + (float)sub_size.x();
        u0 = (atlas_u_lo + 0.5f) / atlas_w;
        u1 = (atlas_u_hi - 0.5f) / atlas_w;
        v0 = (atlas_v_lo + 0.5f) / atlas_h;
        v1 = (atlas_v_hi - 0.5f) / atlas_h;
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
    constexpr struct {
        u8 a:2, b:2, c:2, d:2;
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
    const auto full_w = (u32)sprite.width_minus1 + 1;
    const auto full_h = (u32)sprite.height_minus1 + 1;
    return texcoords_for_subrect(atlas, sprite, {0, 0}, {full_w, full_h}, mirror);
}

void dump_atlas(Atlas& atlas, StringView out_path)
{
    fm_assert(!atlas.layers.isEmpty());
    flush(atlas);

    // stb's image-write encoders count bytes in an `int`, so a file past 2 GB of raw pixels
    // (512 MP at RGBA8) overflows. Each file gets 128 MP, a quarter of that, leaving headroom for
    // compression workspace. atlas.png becomes atlas.000.png, atlas.001.png, ...
    constexpr u64 pixels_per_chunk = u64{1024} * 1024 * 128;

    const auto split = Utility::Path::splitExtension(out_path);
    const auto base = split.first();
    const auto ext = split.second();
    const auto w = (Int)atlas.layer_width, h = (Int)atlas.layer_height;
    const auto n = (Int)atlas.texture_layer_count;
    const Int layers_per_chunk = Math::max<Int>(1,
        (Int)(pixels_per_chunk / (u64(w) * u64(h))));

    PluginManager::Manager<Trade::AbstractImageConverter> mgr;
    auto conv = mgr.loadAndInstantiate("AnyImageConverter");
    fm_assert(conv);

    const Int n_chunks = (n + layers_per_chunk - 1) / layers_per_chunk;
    for (Int c = 0; c < n_chunks; c++)
    {
        const Int first = c * layers_per_chunk;
        const Int count = Math::min<Int>(layers_per_chunk, n - first);

        auto chunk = atlas.texture.subImage(0,
            Range3Di{{0, 0, first}, {w, h, first + count}},
            Image3D{PixelFormat::RGBA8Unorm});
        // Tightly-packed 3D storage → re-view as `w` wide, `count*h` tall
        // 2D image, same bytes. Vertical stack of `count` layers.
        ImageView2D stitched{chunk.format(),
                             {w, count * h},
                             chunk.data()};

        char path_buf[1024];
        const int written = std::snprintf(path_buf, sizeof(path_buf),
            "%.*s.%03d%.*s",
            (int)base.size(), base.data(), c,
            (int)ext.size(), ext.data());
        fm_assert(written > 0 && (usize)written < sizeof(path_buf));
        auto dst = StringView{path_buf, (usize)written,
                              StringViewFlag::NullTerminated};
        if (!conv->convertToFile(stitched, dst))
            fm_throw("can't write atlas chunk {} to '{}'", c, dst);
    }
}

void dump_sprite(Atlas& atlas, const Sprite& sprite, StringView out_path)
{
    // sprite.width_minus1/height_minus1 always describe the ORIGINAL dims. When the packer
    // stores the sprite rotated, the atlas slot is transposed
    // (slot_w = orig_h, slot_h = orig_w); read the slot and un-rotate
    // before writing so the output matches what the caller expects.
    const u32 orig_w = (u32)sprite.width_minus1 + 1;
    const u32 orig_h = (u32)sprite.height_minus1 + 1;
    const bool rotated = sprite.is_rotated != 0;
    const u32 slot_w = rotated ? orig_h : orig_w;
    const u32 slot_h = rotated ? orig_w : orig_h;
    const Int x = (Int)sprite.x;
    const Int y = (Int)sprite.y;
    const Int layer = (Int)sprite.layer;

    flush(atlas);
    auto img = atlas.texture.subImage(0,
        Range3Di{{x, y, layer}, {x + (Int)slot_w, y + (Int)slot_h, layer + 1}},
        Image3D{PixelFormat::RGBA8Unorm});

    const Vector2i dst_size{(Int)orig_w, (Int)orig_h};
    Array<u32> unrotated;
    ArrayView<const char> pixels = img.data();
    if (rotated)
    {
        // Invert upload_sprite's 90° CCW.
        unrotated = Array<u32>{NoInit, (usize)orig_w * (usize)orig_h};
        const auto* slot = reinterpret_cast<const u32*>(img.data().data());
        for (u32 y_o = 0; y_o < orig_h; y_o++)
        {
            u32* dst_row = unrotated.data() + (usize)y_o * (usize)orig_w;
            const u32 slot_col = orig_h - 1 - y_o;
            for (u32 x_o = 0; x_o < orig_w; x_o++)
                dst_row[x_o] = slot[(usize)x_o * (usize)slot_w + (usize)slot_col];
        }
        pixels = {reinterpret_cast<const char*>(unrotated.data()),
                  unrotated.size() * sizeof(u32)};
    }

    ImageView2D view{img.format(), dst_size, pixels};

    PluginManager::Manager<Trade::AbstractImageConverter> mgr;
    auto conv = mgr.loadAndInstantiate("AnyImageConverter");
    fm_assert(conv);
    if (!conv->convertToFile(view, out_path))
        fm_throw("can't write sprite dump to '{}'", out_path);
}


} // namespace floormat::SpriteAtlas

namespace floormat {

sprite::sprite(const SpriteAtlas::Sprite* s) : _s{s} { fm_assert(_s); }

u32 sprite::x() const           { return (u32)_s->x; }
u32 sprite::y() const           { return (u32)_s->y; }
u32 sprite::layer() const       { return (u32)_s->layer; }
u32 sprite::width() const       { return (u32)_s->width_minus1 + 1; }
u32 sprite::height() const      { return (u32)_s->height_minus1 + 1; }
bool     sprite::is_rotated() const  { return _s->is_rotated != 0; }
const SpriteAtlas::Sprite* sprite::raw() const { return _s; }

sprite_atlas::sprite_atlas()
    : _atlas{InPlaceInit}
{
    // The first add() sets the layer size, because this runs during static init, before a GL context exists.
}

sprite_atlas::sprite_atlas(u16 layer_width, u16 layer_height)
    : _atlas{InPlaceInit}
{
    _atlas->layer_width = layer_width;
    _atlas->layer_height = layer_height;
}

sprite_atlas::~sprite_atlas() noexcept = default;
sprite_atlas::sprite_atlas(sprite_atlas&&) noexcept = default;
sprite_atlas& sprite_atlas::operator=(sprite_atlas&&) noexcept = default;

u16 sprite_atlas::layer_width() const  { return _atlas->layer_width; }
u16 sprite_atlas::layer_height() const { return _atlas->layer_height; }
u16 sprite_atlas::layer_count() const  { return (u16)_atlas->layers.size(); }
SpriteAtlas::Atlas* sprite_atlas::raw() { return _atlas.get(); }
const SpriteAtlas::Atlas* sprite_atlas::raw() const { return _atlas.get(); }

sprite sprite_atlas::add(const ImageView2D& pixels, bool allow_rotate)
{
    if (_atlas->layer_width == 0)
    {
        u32 w = SpriteAtlas::max_layer_width, h = SpriteAtlas::max_layer_height;
        if (GL::Context::hasCurrent())
        {
            const auto max_size = SpriteAtlas::max_2d_texture_size();
            w = Math::min(w, max_size);
            h = Math::min(h, max_size);
        }
        fm_assert(w > 0 && h > 0);
        _atlas->layer_width = (u16)w;
        _atlas->layer_height = (u16)h;
    }
    auto size = pixels.size();
    auto* s = SpriteAtlas::alloc_sprite(*_atlas,
                                        (u32)size.x(),
                                        (u32)size.y(),
                                        allow_rotate);
    SpriteAtlas::stage_sprite(*_atlas, *s, pixels);
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
    if (!_atlas->pending.isEmpty() || _atlas->texture_layer_count < _atlas->layers.size()) [[unlikely]]
        SpriteAtlas::flush(*_atlas);
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
