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
    const SpriteAtlas::Sprite* _s;

public:
    sprite(const SpriteAtlas::Sprite* s);

    const SpriteAtlas::Sprite* raw() const;

    u32 x() const;
    u32 y() const;
    u32 layer() const;
    u32 width() const;
    u32 height() const;
    bool is_rotated() const;
};

class sprite_atlas
{
    Pointer<SpriteAtlas::Atlas> _atlas;

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

    // Writes the layers stacked vertically into files a.000.png, a.001.png, ... for an out_filename of a.png.
    void dump(StringView out_filename);

    // Writes s to out_filename in its original orientation.
    void dump_sprite(sprite s, StringView out_filename);

    SpriteAtlas::Atlas* raw();
    const SpriteAtlas::Atlas* raw() const;
};

} // namespace floormat
