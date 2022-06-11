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

    setUniform(TextureUnit, TextureUnit);
}

tile_shader& tile_shader::bindTexture(GL::Texture2D& texture)
{
    texture.bind(TextureUnit);
    return *this;
}

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    setUniform(ProjectionUniform, scale);
    return *this;
}

} // namespace Magnum::Examples
