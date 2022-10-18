#include "shaders/tile-shader.hpp"
#include "loader.hpp"
#include "compat/assert.hpp"
#include <algorithm>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>

namespace floormat {

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
    set_tint({1, 1, 1, 1});
}

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    if (scale != _scale)
        setUniform(ScaleUniform, _scale = scale);
    return *this;
}

tile_shader& tile_shader::set_camera_offset(Vector2d camera_offset)
{
    static constexpr auto MAX = std::numeric_limits<std::int32_t>::max();
    ASSERT(std::fabs(camera_offset[0]) <= MAX);
    ASSERT(std::fabs(camera_offset[1]) <= MAX);
    if (camera_offset != _camera_offset)
    {
        _camera_offset = camera_offset;
        setUniform(OffsetUniform, Vector2i{std::int32_t(camera_offset[0]*2), std::int32_t(camera_offset[1]*2)});
    }

    return *this;
}

tile_shader& tile_shader::set_tint(const Vector4& tint)
{
    if (tint != _tint)
        setUniform(TintUniform, _tint = tint);
    return *this;
}

} // namespace floormat
