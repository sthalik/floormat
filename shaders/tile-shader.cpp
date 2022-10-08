#include "shaders/tile-shader.hpp"
#include "loader.hpp"
#include <algorithm>
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
    CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile());
    CORRADE_INTERNAL_ASSERT_OUTPUT(frag.compile());
    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    set_scale({640, 480});
    set_camera_offset({0, 0});
}

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    if (scale != scale_)
        setUniform(ScaleUniform, scale_ = scale);
    return *this;
}

tile_shader& tile_shader::set_camera_offset(Vector2 camera_offset)
{
    CORRADE_INTERNAL_ASSERT(std::fabs(camera_offset[0]) <= std::scalbn(1.f, std::numeric_limits<float>::digits));
    CORRADE_INTERNAL_ASSERT(std::fabs(camera_offset[1]) <= std::scalbn(1.f, std::numeric_limits<float>::digits));
    if (camera_offset != camera_offset_)
        setUniform(OffsetUniform, camera_offset_ = camera_offset);
    return *this;
}

tile_shader& tile_shader::set_tint(const Color4& tint)
{
    if (tint != tint_)
        setUniform(TintUniform, tint_ = tint);
    return *this;
}

} // namespace Magnum::Examples
