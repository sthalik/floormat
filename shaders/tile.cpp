#include "shaders/tile.hpp"
#include "loader.hpp"
#include "compat/assert.hpp"
#include <Magnum/Math/Vector4.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>

namespace floormat {

tile_shader::tile_shader()
{
    using V = GL::Version;
    const auto version = GL::Context::current().supportedVersion({ V::GL460, V::GL450, V::GL440, });

    if (version < GL::Version::GL430)
        fm_abort("floormat requires OpenGL version 430, only %d is supported", (int)version);

    GL::Shader vert{version, GL::Shader::Type::Vertex};
    GL::Shader frag{version, GL::Shader::Type::Fragment};

    vert.addSource(loader.shader("shaders/tile.vert"));
    frag.addSource(loader.shader("shaders/tile.frag"));
    CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile());
    CORRADE_INTERNAL_ASSERT_OUTPUT(frag.compile());
    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    set_scale({640, 480});
    set_camera_offset({0, 0});
    set_tint({1, 1, 1, 1});
}

tile_shader::~tile_shader() = default;

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    if (scale != _scale)
        setUniform(ScaleUniform, _scale = scale);
    return *this;
}

tile_shader& tile_shader::set_camera_offset(Vector2d camera_offset)
{
    _camera_offset = camera_offset;
    return *this;
}

tile_shader& tile_shader::set_tint(const Vector4& tint)
{
    if (tint != _tint)
        setUniform(TintUniform, _tint = tint);
    return *this;
}

void tile_shader::_draw()
{
    if (const auto offset = Vector2{(float)_camera_offset[0], (float)_camera_offset[1]};
        offset != _real_camera_offset)
    {
        fm_assert(offset[0] < 1 << 24 && offset[1] < 1 << 24);
        _real_camera_offset = offset;
        setUniform(OffsetUniform, offset);
    }
}

} // namespace floormat
