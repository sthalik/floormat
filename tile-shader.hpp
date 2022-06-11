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
    tile_shader& set_scale(const Vector2& scale);
    tile_shader& bindTexture(GL::Texture2D& texture);

private:
    enum { TextureUnit = 0 };
    enum { ProjectionUniform = 0, };
};

} // namespace Magnum::Examples
