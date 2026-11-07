#pragma once
#include <array>
#include <cr/Array.h>
#include <cr/Pointer.h>
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
    u32 height;
};

// A sprite added but not yet in the texture: its pixels sit at `staging_offset` in Atlas::staging, tightly
// packed in the slot's orientation.
struct PendingSprite
{
    const Sprite* sprite;
    PixelFormat format;
    u32 staging_offset;
};

struct Atlas
{
    Array<Layer> layers {};
    Array<Pointer<HeightClass>> height_classes {};
    // Added sprites wait here until texture() needs them, so adding needs no GL context.
    Array<PendingSprite> pending {};
    Array<char> staging {};
    GL::Texture2DArray texture{NoCreate};
    u16 layer_width = 0, layer_height = 0;
    u16 texture_layer_count = 0;
    u32 texture_gen = 0;
};

u32 quantize_height(u32 h);
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
void dump_atlas(Atlas& atlas, StringView out_path);
void dump_sprite(Atlas& atlas, const Sprite& sprite, StringView out_path);

} // namespace floormat::SpriteAtlas
