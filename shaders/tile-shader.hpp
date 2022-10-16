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

private:
    Vector2 scale_, camera_offset_;
    Vector4 tint_;

    enum { ScaleUniform = 0, OffsetUniform = 1, TintUniform = 2, };
};

} // namespace floormat
