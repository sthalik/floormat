#pragma once
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Matrix4.h>

namespace Magnum::Examples {

struct tile_shader : GL::AbstractShaderProgram
{
    typedef GL::Attribute<0, Vector3> Position;
    typedef GL::Attribute<1, Vector2> TextureCoordinates;

    explicit tile_shader();
    Vector2 scale() const { return scale_; }
    Vector2 project(Vector3 pt) const;
    tile_shader& set_scale(const Vector2& scale);
    tile_shader& set_camera_offset(Vector2 camera_offset);
    tile_shader& bindTexture(GL::Texture2D& texture);

private:
    Vector2 scale_, camera_offset_;
    enum { TextureUnit = 0 };
    enum { ScaleUniform = 0, OffsetUniform = 3, };
};

} // namespace Magnum::Examples
