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
    uint64_t y : 14, height_class : 10, next_x : 14;

    uint64_t _pad0 : 26 = 0;
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
    uint16_t layer_size;
    uint16_t n_layers = 0;

    struct ShelfPair { Shelf* p; uint32_t index; };
};

uint16_t alloc_more_layers_count(uint16_t cur_layers, const Atlas& A);
void realloc_atlas(Atlas& atlas, uint16_t new_n_layers);
void free_atlas(Atlas& atlas);
Atlas::ShelfPair alloc_new_shelf(Atlas& atlas, uint32_t height);
Sprite* alloc_sprite(Atlas& atlas, uint32_t w, uint32_t h, bool allow_rotate = false);
void upload_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels);
std::array<Vector3, 4> texcoords_for_sprite(const Atlas& atlas, const Sprite& sprite, bool mirror);
void dump_atlas(Atlas& atlas, StringView out_path);
void dump_sprite(Atlas& atlas, const Sprite& sprite, StringView out_path);

} // namespace floormat:SpriteAtlas
