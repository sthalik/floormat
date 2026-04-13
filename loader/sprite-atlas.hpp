#pragma once
#include <array>
#include <cr/Pointer.h>
#include <mg/Vector3.h>

namespace floormat::SpriteAtlas {
struct Atlas;
struct Sprite;
} // namespace floormat::SpriteAtlas

namespace floormat {

class sprite
{
public:
    sprite(const SpriteAtlas::Sprite* s);

    const SpriteAtlas::Sprite* raw() const { return _s; }

    uint32_t x() const;
    uint32_t y() const;
    uint32_t layer() const;
    uint32_t width() const;
    uint32_t height() const;
    bool is_rotated() const;

private:
    const SpriteAtlas::Sprite* _s;
};

class sprite_atlas
{
public:
    explicit sprite_atlas(uint16_t layer_size);
    ~sprite_atlas();

    sprite_atlas(const sprite_atlas&) = delete;
    sprite_atlas& operator=(const sprite_atlas&) = delete;
    sprite_atlas(sprite_atlas&&) noexcept;
    sprite_atlas& operator=(sprite_atlas&&) noexcept;

    uint16_t layer_size() const;
    uint16_t n_layers() const;

    sprite add(const ImageView2D& pixels, bool allow_rotate = false);

    std::array<Vector3, 4> texcoords_for(sprite s, bool mirror) const;

    // Release the GL texture and bookkeeping. layer_size is preserved.
    void reset();

    SpriteAtlas::Atlas* raw() { return _atlas.get(); }
    const SpriteAtlas::Atlas* raw() const { return _atlas.get(); }

private:
    Pointer<SpriteAtlas::Atlas> _atlas;
};

} // namespace floormat
