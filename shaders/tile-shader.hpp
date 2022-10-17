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
    Vector2 camera_offset() const { return camera_offset_; }
    tile_shader& set_camera_offset(Vector2 camera_offset);
    Vector4 tint() const { return tint_; }
    tile_shader& set_tint(const Vector4& tint);

    static constexpr Vector2 project(Vector3 pt);
    static constexpr Vector2 unproject(Vector2 px);

private:
    Vector2 scale_, camera_offset_;
    Vector4 tint_;

    enum { ScaleUniform = 0, OffsetUniform = 1, TintUniform = 2, };
};

constexpr Vector2 tile_shader::project(const Vector3 pt)
{
    const float x = -pt[1], y = -pt[0], z = pt[2];
    return { x-y, (x+y+z*2)*.59f };
}

constexpr Vector2 tile_shader::unproject(const Vector2 px)
{
    const float X = px[0], Y = px[1];
    return { X/2 + 50.f * Y / 59, 50 * Y / 59 - X/2 };
}

} // namespace floormat
