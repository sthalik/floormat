#pragma once
#include "src/rotation.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

struct wall_atlas;

struct wall_frame
{
    Vector2ui offset = { (unsigned)-1, (unsigned)-1 };
};

struct wall_frames
{
    ArrayView<const wall_frame> items(const wall_atlas& a) const;

    uint32_t index = (uint32_t)-1, count = (uint32_t)-1; // not serialized

    Vector2ui pixel_size;
    Color4 tint_mult{1,1,1,1};
    Color3 tint_add;
    uint8_t from_rotation = (uint8_t)-1;
    bool mirrored : 1 = false;
};

struct wall_frame_set
{
    wall_frames wall, overlay, side, top;
    wall_frames corner_L, corner_R;
};

struct wall_info
{
    String name = "(unnamed)"_s;
    float depth = 1;
};

struct wall_atlas final
{
    wall_atlas();
    wall_atlas(wall_info info,
               const ImageView2D& image,
               ArrayView<const wall_frame_set> rotations,
               ArrayView<const wall_frame> frames);
    ~wall_atlas() noexcept;
    static size_t enum_to_index(enum rotation x);
    const wall_frame_set& frameset(size_t i) const;
    const wall_frame_set& frameset(enum rotation r) const;
    const ArrayView<const wall_frame> array() const;
    StringView name() const;

private:
    String _name;
    std::array<wall_frame_set, 4> _rotations;
    Array<wall_frame> _array;
    GL::Texture2D _texture;
    wall_info _info;
    uint8_t _rotation_count = 0;
};

} // namespace floormat
