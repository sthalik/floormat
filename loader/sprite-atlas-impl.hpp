#pragma once
#include <array>
#include <cr/Array.h>
#include <cr/Pointer.h>
#include <mg/TextureArray.h>
#include <mg/Vector3.h>

namespace floormat::SpriteAtlas {

constexpr inline uint32_t size_class_size = 8;
//constexpr inline uint16_t n_size_classes = max_texture_xy/size_class_size;

struct Sprite {
    // store width/height - 1 so that exactly 1024x1024 fits as 1023x1023.
    // before that assert that stored size isn't 0.
    uint64_t x : 14, y : 14, layer : 14, width : 10, height : 10;
    uint64_t is_rotated : 1;

    uint64_t _pad0 : 1 = 0;
};

struct Shelf
{
    Array<Pointer<Sprite>> sprites {};
    // next_x needs 15 bits: it represents one-past-end and must reach
    // layer_size (up to 16384, the runtime cap on 14-bit Sprite::x). A
    // 14-bit next_x truncates 16384 to 0 and re-allocates from x=0,
    // causing every sprite past the shelf-full boundary to collide with
    // the first sprite on the same shelf.
    uint64_t y : 14, height_class : 10, next_x : 15;

    uint64_t _pad0 : 25 = 0;
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

struct Atlas
{
    Array<Layer> layers {};
    Array<Pointer<HeightClass>> height_classes {};
    GL::Texture2DArray texture{NoCreate};
    uint16_t layer_size = 0;
    uint16_t n_layers = 0;

    struct ShelfPair { Shelf* p; uint32_t index; };
};

uint32_t max_2d_texture_size();
uint16_t alloc_more_layers_count(uint16_t cur_layers, const Atlas& A);
void realloc_atlas(Atlas& atlas, uint16_t new_n_layers);
void free_atlas(Atlas& atlas);
Atlas::ShelfPair alloc_new_shelf(Atlas& atlas, uint32_t height);
Sprite* alloc_sprite(Atlas& atlas, uint32_t w, uint32_t h, bool allow_rotate = false);
void upload_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels);
std::array<Vector3, 4> texcoords_for_sprite(const Atlas& atlas, const Sprite& sprite, bool mirror);
std::array<Vector3, 4> texcoords_for_subrect(const Atlas& atlas, const Sprite& sprite,
                                             Vector2ui sub_offset, Vector2ui sub_size,
                                             bool mirror);
void dump_atlas(Atlas& atlas, StringView out_path);
void dump_sprite(Atlas& atlas, const Sprite& sprite, StringView out_path);

} // namespace floormat:SpriteAtlas
