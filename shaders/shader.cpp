#include "shaders/shader.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "local-coords.hpp"
#include <cmath>
#include <Corrade/Containers/Iterable.h>
#include <Corrade/Containers/StringStl.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>

namespace floormat {

tile_shader::tile_shader()
{
    constexpr auto min_version = GL::Version::GL330;
    const auto version = GL::Context::current().version();

    if (version < min_version)
        fm_abort("floormat requires OpenGL version %d, only %d is supported", (int)min_version, (int)version);

    GL::Shader vert{version, GL::Shader::Type::Vertex};
    GL::Shader frag{version, GL::Shader::Type::Fragment};

    vert.addSource(loader.shader("shaders/shader.vert"));
    frag.addSource(loader.shader("shaders/shader.frag"));
    CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile());
    CORRADE_INTERNAL_ASSERT_OUTPUT(frag.compile());
    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    set_scale({640, 480});
    set_tint({1, 1, 1, 1});
    setUniform(OffsetUniform, Vector3{});
}

tile_shader::~tile_shader() = default;

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    if (scale != _scale)
        setUniform(ScaleUniform, 2.f/(_scale = scale));
    return *this;
}

tile_shader& tile_shader::set_camera_offset(const Vector2d& camera_offset, float depth_offset)
{
    _camera_offset = camera_offset;
    _depth_offset = depth_offset;
    return *this;
}

tile_shader& tile_shader::set_tint(const Vector4& tint)
{
    _tint = tint;
    return *this;
}

void tile_shader::_draw()
{
    fm_assert(std::fabs(_camera_offset[0]) < 1 << 24 && std::fabs(_camera_offset[1]) < 1 << 24);
    fm_assert(std::fabs(_depth_offset) < 1 << 24);

    if (_tint != _real_tint)
        setUniform(TintUniform, _real_tint = _tint);

    if (const auto offset = Vector3(Vector2(_camera_offset), _depth_offset);
        offset != _real_camera_offset)
    {
        _real_camera_offset = offset;
        setUniform(OffsetUniform, offset);
    }
}

float tile_shader::depth_value(const local_coords& xy, float offset) noexcept
{
    return ((float)xy.x + (float)xy.y + offset) * depth_tile_size;
}

const Vector2s tile_shader::max_screen_tiles = {8, 8};
const float tile_shader::character_depth_offset = 1 + 1./64;
const float tile_shader::scenery_depth_offset = 1 + 1./64;
const float tile_shader::wall_depth_offset = 1;
const float tile_shader::z_depth_offset = 1 + 2./64;
const float tile_shader::depth_tile_size = 1/(double)(TILE_MAX_DIM * 2 * max_screen_tiles.product());

} // namespace floormat
