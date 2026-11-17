#pragma once
#include "sprite-constants.hpp"
#include "compat/assert.hpp"
#include <array>
#include <bit>
#include <cr/Array.h>
#include <cr/Pointer.h>
#include <mg/Functions.h>
#include <mg/TextureArray.h>
#include <mg/Vector3.h>

namespace floormat::SpriteAtlas {

struct Sprite {
    u64 x : 14, y : 14, layer : 14, width_minus1 : 10, height_minus1 : 10;
    u64 is_rotated : 1;

    u64 _pad0 : 1 = 0;
};

struct Shelf
{
    Array<Pointer<Sprite>> sprites {};
    // One past the shelf's last used column, so it can equal layer_width. 15 bits hold 1 << 14,
    // the widest layer the 14-bit Sprite::x can address.
    u64 y : 14, next_x : 15, layer : 14;

    u64 _pad0 : 21 = 0;
};

struct Layer
{
    Array<Pointer<Shelf>> shelves {};
    u16 next_y = 0;
};

struct HeightClass
{
    Array<Shelf*> shelves {};
};

// A sprite added but not yet in the texture: its pixels sit at `staging_offset` in Atlas::staging, tightly
// packed in the slot's orientation.
struct PendingSprite
{
    const Sprite* sprite;
    PixelFormat format;
    u32 staging_offset;
};

constexpr inline u32 size_class_divisor = 16; // height classes per octave = divisor/2 (recommended: 16)
constexpr inline u32 min_height_class = 4;

constexpr u32 quantize_height(u32 h)
{
    fm_debug_assert(h > 0);
    u32 bucket = Math::max(min_height_class, std::bit_ceil(h) / size_class_divisor);
    return (h + bucket - 1) / bucket * bucket;
}

constexpr inline auto class_heights = [] {
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
    std::array<u32, count> heights{};
    u32 filled = 0;
    for (u32 h = 1; h <= max_sprite_xy; h++)
        if (const u32 q = quantize_height(h); filled == 0 || q != heights[filled-1])
            heights[filled++] = q;
    fm_assert(filled == count);
    fm_assert(heights[count-1] == max_sprite_xy);
    std::array<u16, count> ret{};
    for (u32 i = 0; i < count; i++)
    {
        fm_assert(heights[i] == (u16)heights[i]);
        ret[i] = (u16)heights[i];
    }
    return ret;
}();

constexpr inline std::array<u8, max_sprite_xy> height_class_indices = [] {
    std::array<u32, max_sprite_xy> indices{};
    u32 index = 0;
    for (u32 h = 1; h <= max_sprite_xy; h++)
    {
        if (quantize_height(h) != class_heights[index])
            index++;
        indices[h-1] = index;
    }
    fm_assert(indices[max_sprite_xy-1] == class_heights.size()-1);
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
    std::array<HeightClass, class_heights.size()> height_classes {};
    // Added sprites wait here until texture() needs them, so adding needs no GL context.
    Array<PendingSprite> pending {};
    Array<char> staging {};
    GL::Texture2DArray texture{NoCreate};
    u16 layer_width = 0, layer_height = 0;
    u16 texture_layer_count = 0;
    u32 texture_gen = 0;
};

u32 max_2d_texture_size();
u16 get_next_layer_count(u16 layer_count, const Atlas& A);
void grow_texture(Atlas& atlas, u16 new_layer_count);
void ensure_texture(Atlas& atlas);
void free_atlas(Atlas& atlas);
Shelf* alloc_new_shelf(Atlas& atlas, u32 height);
Sprite* alloc_sprite(Atlas& atlas, u32 w, u32 h, bool allow_rotate = true);
void upload_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels);
void stage_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels);
void flush(Atlas& atlas);
std::array<Vector3, 4> texcoords_for_sprite(const Atlas& atlas, const Sprite& sprite, bool mirror);
std::array<Vector3, 4> texcoords_for_subrect(const Atlas& atlas, const Sprite& sprite,
                                             Vector2ui sub_offset, Vector2ui sub_size,
                                             bool mirror);
Image2D read_layer_pixels(GL::Texture2DArray& texture, Int layer, Range2Di rect);
void read_layer_pixels(GL::Texture2DArray& texture, Int layer, Range2Di rect, const MutableImageView2D& out);
GL::Texture2D copy_slot(Atlas& atlas, const Sprite& sprite);
void dump_atlas(Atlas& atlas, StringView out_filename);
void dump_sprite(Atlas& atlas, const Sprite& sprite, StringView out_filename);

} // namespace floormat::SpriteAtlas
