#pragma once
#include <array>
#include <cr/Array.h>
#include <cr/Pointer.h>
#include <mg/TextureArray.h>
#include <mg/Vector3.h>

namespace floormat::SpriteAtlas {

struct Sprite {
    uint64_t x : 14, y : 14, layer : 14, width_minus1 : 10, height_minus1 : 10;
    uint64_t is_rotated : 1;

    uint64_t _pad0 : 1 = 0;
};

struct Shelf
{
    Array<Pointer<Sprite>> sprites {};
    // next_x needs 15 bits: it represents one-past-end and must reach
    // layer_width (up to 16384, the runtime cap on 14-bit Sprite::x). A
    // 14-bit next_x truncates 16384 to 0 and re-allocates from x=0,
    // causing every sprite past the shelf-full boundary to collide with
    // the first sprite on the same shelf.
    uint64_t y : 14, next_x : 15, layer : 14;

    uint64_t _pad0 : 21 = 0;
};

struct Layer
{
    Array<Pointer<Shelf>> shelves {};
    uint16_t next_y = 0;
};

struct HeightClass
{
    Array<Shelf*> shelves {};
    uint32_t height;
};

// A sprite added but not yet in the texture: its pixels sit at `staging_offset` in Atlas::staging, tightly
// packed in the slot's orientation.
struct PendingSprite
{
    const Sprite* sprite;
    PixelFormat format;
    uint32_t staging_offset;
};

struct Atlas
{
    Array<Layer> layers {};
    Array<Pointer<HeightClass>> height_classes {};
    // Added sprites wait here until texture() needs them, so adding needs no GL context.
    Array<PendingSprite> pending {};
    Array<char> staging {};
    GL::Texture2DArray texture{NoCreate};
    uint16_t layer_width = 0, layer_height = 0;
    uint16_t texture_layer_count = 0;
};

uint32_t quantize_height(uint32_t h);
uint32_t max_2d_texture_size();
uint16_t get_next_layer_count(uint16_t layer_count, const Atlas& A);
void grow_texture(Atlas& atlas, uint16_t new_layer_count);
void ensure_texture(Atlas& atlas);
void free_atlas(Atlas& atlas);
Shelf* alloc_new_shelf(Atlas& atlas, uint32_t height);
Sprite* alloc_sprite(Atlas& atlas, uint32_t w, uint32_t h, bool allow_rotate = true);
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
