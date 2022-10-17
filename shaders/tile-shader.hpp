#pragma once
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

namespace floormat {

struct tile_shader : GL::AbstractShaderProgram
{
    typedef GL::Attribute<0, Vector3> Position;
    typedef GL::Attribute<1, Vector2> TextureCoordinates;

    explicit tile_shader();

    Vector2 scale() const { return scale_; }
    tile_shader& set_scale(const Vector2& scale);
    Vector2d camera_offset() const { return camera_offset_; }
    tile_shader& set_camera_offset(Vector2d camera_offset);
    Vector4 tint() const { return tint_; }
    tile_shader& set_tint(const Vector4& tint);

    static constexpr Vector2d project(Vector3d pt);
    static constexpr Vector2d unproject(Vector2d px);

private:
    Vector2d camera_offset_;
    Vector2 scale_;
    Vector4 tint_;

    enum { ScaleUniform = 0, OffsetUniform = 1, TintUniform = 2, };
};

constexpr Vector2d tile_shader::project(const Vector3d pt)
{
    const auto x = -pt[0]*.5, y = pt[1]*.5, z = pt[2];
    return { (x-y), (x+y+z)*.59 };
}

constexpr Vector2d tile_shader::unproject(const Vector2d px)
{
    const auto X = px[0], Y = px[1];
    return { X/2 + 50 * Y / 59, 50 * Y / 59 - X/2 };
}

} // namespace floormat
