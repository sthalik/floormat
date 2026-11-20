#pragma once
#include "sprite-atlas.hpp"
#include "sprite-constants.hpp"
#include "compat/assert.hpp"
#include <array>
#include <bit>
#include <cr/Array.h>
#include <cr/Optional.h>
#include <mg/TextureArray.h>

namespace floormat::SpriteAtlas {

constexpr inline u32 size_class_divisor = 16; // height classes per octave = divisor/2 (recommended: 16)
constexpr inline u32 min_height_class_pixels = 4;

struct Sprite {
    u32 x, y, layer, width, height;
    bool is_rotated;
};

struct Shelf
{
    Array<Pointer<Sprite>> sprites {};
    u32 y, next_x, layer;
};

struct Layer
{
    Array<Pointer<Shelf>> shelves {};
    u32 next_y = 0;
};

struct HeightClass
{
    Array<Shelf*> shelves {};
};

// A sprite added but not yet in the texture: its pixels sit at `staging_offset` in Atlas::staging, tightly
// packed in the rect's orientation.
struct PendingSprite
{
    const Sprite* sprite;
    PixelFormat format;
    u32 staging_offset;
};

constexpr u32 quantize_height(u32 h)
{
    fm_debug_assert(h > 0);
    u32 bucket = Math::max(min_height_class_pixels, std::bit_ceil(h) / size_class_divisor);
    return (h + bucket - 1) / bucket * bucket;
}

constexpr inline auto height_classes = [] {
    constexpr u32 count = [] {
        u32 n = 0, prev = 0;
        for (u32 h = 1; h <= max_sprite_xy; h++)
        {
            const u32 q = quantize_height(h);
            fm_assert(q >= prev);
            n += q != prev;
            prev = q;
        }
        return n;
    }();
    std::array<u16, count> heights;
    u32 filled = 0;
    for (u32 h = 1; h <= max_sprite_xy; h++)
        if (const auto q = quantize_height(h); filled == 0 || q != heights[filled-1])
        {
            using type = decltype(heights)::value_type;
            fm_assert(q == type(q));
            heights[filled++] = (type)q;
        }
    fm_assert(filled == count);
    fm_assert(heights.back() == max_sprite_xy);
    return heights;
}();

constexpr inline std::array<u8, max_sprite_xy> height_class_indices = [] {
    std::array<u32, max_sprite_xy> indices{};
    u32 index = 0;
    for (u32 h = 1; h <= max_sprite_xy; h++)
    {
        if (quantize_height(h) != height_classes[index])
            index++;
        indices[h-1] = index;
    }
    fm_assert(indices[max_sprite_xy-1] == height_classes.size()-1);
    std::array<u8, max_sprite_xy> ret{};
    for (u32 i = 0; i < max_sprite_xy; i++)
    {
        fm_assert(indices[i] == (u8)indices[i]);
        ret[i] = (u8)indices[i];
    }
    return ret;
}();

constexpr u32 height_class_index(u32 h)
{
    fm_debug_assert(h > 0 && h <= max_sprite_xy);
    return height_class_indices[h-1];
}

struct Atlas
{
    Array<Layer> layers {};
    std::array<HeightClass, SpriteAtlas::height_classes.size()> height_classes {};
    // Added sprites wait here until texture() needs them, so adding needs no GL context.
    Array<PendingSprite> pending {};
    Array<char> staging {};
    GL::Texture2DArray texture{NoCreate};
    u32 layer_width = 0, layer_height = 0;
    u32 texture_layer_count = 0; // allocated layers
    u32 texture_layer_capacity = 0; // layers in texture's storage, committed or not
    u32 uploaded_layer_count = 0; // sprites added before the last flush()
    u32 texture_gen = 0;
    layer_copy_mode layer_copy = layer_copy_mode::blit;

    struct Limits
    {
        u32 size = 0, layer_count = 0;
        u32 sparse_texture_size = 0, sparse_array_texture_layer_count = 0; // 0 without ARB_sparse_texture
        void ensure_filled();
    } limits;
};

Atlas::Limits get_texture_limits();
// Needs ARB_sparse_texture and ARB_internalformat_query.
Optional<Int> find_virtual_page_size_index(const Atlas& atlas);
// Returns whether sparse storage for the atlas can hold layer_count layers. Needs filled limits.
bool can_make_sparse_texture(const Atlas& atlas, u32 layer_count);
u32 get_next_layer_count(u32 layer_count, const Atlas& A);
void grow_texture(Atlas& atlas, u32 new_layer_count);
void ensure_texture(Atlas& atlas);
void free_atlas(Atlas& atlas);
Shelf* alloc_new_shelf(Atlas& atlas, u32 height);
Sprite* alloc_sprite(Atlas& atlas, u32 w, u32 h, bool allow_rotate = true);
void stage_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels);
void flush(Atlas& atlas);
std::array<Vector3, 4> texcoords_for_sprite(const Atlas& atlas, const Sprite& sprite, bool mirror);
std::array<Vector3, 4> texcoords_for_subrect(const Atlas& atlas, const Sprite& sprite,
                                             Vector2ui sub_offset, Vector2ui sub_size,
                                             bool mirror);
Image2D read_layer_pixels(GL::Texture2DArray& texture, Int layer, Range2Di rect);
void read_layer_pixels(GL::Texture2DArray& texture, Int layer, Range2Di rect, const MutableImageView2D& out);
GL::Texture2D copy_rect(Atlas& atlas, const Sprite& sprite);
void dump_atlas(Atlas& atlas, StringView out_filename);
void dump_sprite(Atlas& atlas, const Sprite& sprite, StringView out_filename);

} // namespace floormat::SpriteAtlas
