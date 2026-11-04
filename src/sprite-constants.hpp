#pragma once

namespace floormat::SpriteAtlas {

constexpr inline u16 max_sprite_xy = 1024; // limit on per-sprite width/height
constexpr inline u16 max_layer_width = 4096;
constexpr inline u16 max_layer_height = 1 << 14; // Sprite::y and Shelf::y are 14 bits

} // namespace floormat::SpriteAtlas
