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

    auto& set_color(const Color3& color) { setUniform(_color_uniform, color); return *this; }
    auto& set_projection(const Math::Matrix4<float>& mat) { setUniform(_projection_uniform, mat); return *this; }

    tile_shader& bindTexture(GL::Texture2D& texture);

private:
    enum: Int { TextureUnit = 0 };

    Int _color_uniform, _projection_uniform;
};

} // namespace Magnum::Examples
