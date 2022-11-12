#include "shaders/tile.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "local-coords.hpp"
#include <Corrade/Containers/Iterable.h>
#include <Corrade/Containers/StringStl.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>

namespace floormat {

tile_shader::tile_shader()
{
    using V = GL::Version;
    constexpr V min_version = GL::Version::GL430;
    const auto version = GL::Context::current().supportedVersion({ V::GL460, V::GL450, V::GL440, V::GL430, });

    if (version < min_version)
        fm_abort("floormat requires OpenGL version %d, only %d is supported", (int)min_version, (int)version);

    GL::Shader vert{version, GL::Shader::Type::Vertex};
    GL::Shader frag{version, GL::Shader::Type::Fragment};

    vert.addSource(loader.shader("shaders/tile.vert"));
    frag.addSource(loader.shader("shaders/tile.frag"));
    CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile());
    CORRADE_INTERNAL_ASSERT_OUTPUT(frag.compile());
    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    set_scale({640, 480});
    setUniform(OffsetUniform, Vector2{});
    set_tint({1, 1, 1, 1});
}

tile_shader::~tile_shader() = default;

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    if (scale != _scale)
        setUniform(ScaleUniform, 1.f/(_scale = scale));
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

float tile_shader::depth_value(const local_coords& xy, float offset) noexcept
{
    constexpr float max = (TILE_MAX_DIM+1)*(TILE_MAX_DIM+1) * .5f;
    constexpr float min = -1 + 1.f/256;
    float value = min + (xy.to_index() + offset)/max;
    fm_assert(value > -1 && value < 1);
    return value;
}

} // namespace floormat
