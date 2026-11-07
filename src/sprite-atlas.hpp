#pragma once
#include <array>
#include <cr/Pointer.h>

namespace Magnum::GL { class AbstractTexture; }

namespace floormat::SpriteAtlas {
struct Atlas;
struct Sprite;
} // namespace floormat::SpriteAtlas

namespace floormat {

class sprite
{
public:
    sprite(const SpriteAtlas::Sprite* s);

    const SpriteAtlas::Sprite* raw() const;

    u32 x() const;
    u32 y() const;
    u32 layer() const;
    u32 width() const;
    u32 height() const;
    bool is_rotated() const;

private:
    const SpriteAtlas::Sprite* _s;
};

class sprite_atlas
{
public:
    explicit sprite_atlas();
    explicit sprite_atlas(u16 layer_width, u16 layer_height);
    ~sprite_atlas() noexcept;

    sprite_atlas(const sprite_atlas&) = delete;
    sprite_atlas& operator=(const sprite_atlas&) = delete;
    sprite_atlas(sprite_atlas&&) noexcept;
    sprite_atlas& operator=(sprite_atlas&&) noexcept;

    u16 layer_width() const;
    u16 layer_height() const;
    u16 layer_count() const;
    u32 texture_gen() const;

    sprite add(const ImageView2D& pixels, bool allow_rotate = true);

    std::array<Vector3, 4> texcoords_for(sprite s, bool mirror) const;

    // UVs of a rectangle inside the sprite. `sub_offset` and `sub_size` are in the source frame's
    // top-down coordinates, as JSON frame offsets are. The whole frame is `{0,0}, {width, height}`.
    std::array<Vector3, 4> texcoords_for(sprite s,
                                         Vector2ui sub_offset,
                                         Vector2ui sub_size,
                                         bool mirror) const;

    GL::AbstractTexture& texture();

    // Release the GL texture and bookkeeping. layer_width and layer_height are preserved.
    void reset();

    // Debug: dump each atlas layer (or slabs of layers) to PNG files at
    // `out_path.NNN.png`. Forwards to SpriteAtlas::dump_atlas so callers
    // don't need the impl header.
    void dump(StringView out_path);

    // Debug: dump a single sprite's atlas sub-rect as a PNG at `out_path`.
    // Useful for verifying that the pixels at (sprite.x, sprite.y, layer)
    // actually match what the caller expects to render.
    void dump_sprite(sprite s, StringView out_path);

    SpriteAtlas::Atlas* raw();
    const SpriteAtlas::Atlas* raw() const;

private:
    Pointer<SpriteAtlas::Atlas> _atlas;
};

} // namespace floormat
