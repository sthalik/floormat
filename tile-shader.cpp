#include "tile-shader.hpp"
#include "loader.hpp"

#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>

namespace Magnum::Examples {

tile_shader::tile_shader()
{
    MAGNUM_ASSERT_GL_VERSION_SUPPORTED(GL::Version::GL460);

    GL::Shader vert{GL::Version::GL460, GL::Shader::Type::Vertex};
    GL::Shader frag{GL::Version::GL460, GL::Shader::Type::Fragment};

    vert.addSource(loader.shader("shaders/tile-shader.vert"));
    frag.addSource(loader.shader("shaders/tile-shader.frag"));

    CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({vert, frag}));

    attachShaders({vert, frag});

    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    setUniform(ScaleUniform, Vector2{640, 480});
}

tile_shader& tile_shader::bindTexture(GL::Texture2D& texture)
{
    texture.bind(TextureUnit);
    return *this;
}

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    scale_ = scale;
    setUniform(ScaleUniform, scale);
    return *this;
}
tile_shader& tile_shader::set_camera_offset(Vector2 camera_offset)
{
    camera_offset_ = camera_offset;
    setUniform(OffsetUniform, camera_offset);
    return *this;
}
Vector2 tile_shader::project(Vector3 pt) const
{
    float x = pt[1], y = pt[0], z = pt[2];
    return { x-y+camera_offset_[0], (x+y+z*2)*.75f - camera_offset_[1] };
}

} // namespace Magnum::Examples
