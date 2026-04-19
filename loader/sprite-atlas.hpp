#pragma once
#include <array>
#include <cr/Pointer.h>

namespace Magnum::GL { class AbstractTexture; }

namespace floormat::SpriteAtlas {
struct Atlas;
struct Sprite;

// Limit on per-sprite width/height.
constexpr inline uint16_t max_texture_xy = 1024;
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
    explicit sprite_atlas();
    explicit sprite_atlas(uint16_t layer_size);
    ~sprite_atlas() noexcept;

    sprite_atlas(const sprite_atlas&) = delete;
    sprite_atlas& operator=(const sprite_atlas&) = delete;
    sprite_atlas(sprite_atlas&&) noexcept;
    sprite_atlas& operator=(sprite_atlas&&) noexcept;

    uint16_t layer_size() const;
    uint16_t n_layers() const;

    sprite add(const ImageView2D& pixels, bool allow_rotate = true);

    std::array<Vector3, 4> texcoords_for(sprite s, bool mirror) const;

    // Sub-rect UVs — reads a rectangle inside the sprite rather than the
    // whole sprite. `sub_offset` and `sub_size` are in the source frame's
    // top-down coordinates (same convention as JSON frame offsets). The
    // overload handles the atlas's GL bottom-up storage internally.
    // Whole-frame case is degenerate: `{0,0}, {width, height}`.
    std::array<Vector3, 4> texcoords_for(sprite s,
                                         Vector2ui sub_offset,
                                         Vector2ui sub_size,
                                         bool mirror) const;

    GL::AbstractTexture& texture();

    // Release the GL texture and bookkeeping. layer_size is preserved.
    void reset();

    // Debug: dump each atlas layer (or slabs of layers) to PNG files at
    // `out_path.NNN.png`. Forwards to SpriteAtlas::dump_atlas so callers
    // don't need the impl header.
    void dump(StringView out_path);

    // Debug: dump a single sprite's atlas sub-rect as a PNG at `out_path`.
    // Useful for verifying that the pixels at (sprite.x, sprite.y, layer)
    // actually match what the caller expects to render.
    void dump_sprite(sprite s, StringView out_path);

    SpriteAtlas::Atlas* raw() { return _atlas.get(); }
    const SpriteAtlas::Atlas* raw() const { return _atlas.get(); }

private:
    Pointer<SpriteAtlas::Atlas> _atlas;
};

} // namespace floormat
