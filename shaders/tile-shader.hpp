#pragma once
#include "tile-atlas.hpp"
#include <vector>
#include <utility>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Matrix4.h>

struct tile_atlas;

namespace Magnum::Examples {

struct tile_shader : GL::AbstractShaderProgram
{
    typedef GL::Attribute<0, Vector3> Position;
    typedef GL::Attribute<1, Vector2> TextureCoordinates;

    explicit tile_shader();

    Vector2 scale() const { return scale_; }
    tile_shader& set_scale(const Vector2& scale);
    Vector2 camera_offset() const { return camera_offset_; }
    tile_shader& set_camera_offset(Vector2 camera_offset);

    static inline Vector2 project(Vector3 pt);

private:
    Vector2 scale_, camera_offset_;

    enum { ScaleUniform = 0, OffsetUniform = 1, };
};

Vector2 tile_shader::project(Vector3 pt)
{
    float x = pt[1], y = pt[0], z = pt[2];
    return { x-y, (x+y+z*2)*.75f };
}

} // namespace Magnum::Examples
