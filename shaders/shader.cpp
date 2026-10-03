#include "shader.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "compat/array-size.hpp"
#include "texture-unit-cache.hpp"
#include "src/point.inl"
#include <cr/Iterable.h>
#include <mg/Functions.h>
#include <mg/Vector4.h>
#include <mg/Context.h>
#include <mg/Shader.h>
#include <mg/GL/Version.h>

namespace floormat {

tile_shader::tile_shader(texture_unit_cache& tuc) : tuc{tuc}
{
    constexpr auto min_version = GL::Version::GL330;
    const auto version = GL::Context::current().version();

    if (version < min_version)
        fm_abort("floormat requires OpenGL version %d, only %d is supported", (int)min_version, (int)version);

    GL::Shader vert{min_version, GL::Shader::Type::Vertex};
    GL::Shader frag{min_version, GL::Shader::Type::Fragment};

    vert.addSource(loader.shader("shaders/shader.vert"));
    frag.addSource(loader.shader("shaders/shader.frag"));
    CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile());
    CORRADE_INTERNAL_ASSERT_OUTPUT(frag.compile());
    attachShaders({vert, frag});

    for (auto i = 0u; i < array_size(attribute_names); i++)
        bindAttributeLocation(i, attribute_names[i]);

    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    for (auto i = 0u; i < UNIFORM_COUNT; i++)
    {
        uniform_locations[i] = uniformLocation(uniform_names[i]);
        fm_assert(uniform_locations[i] != -1);
    }

    set_scale({640, 480});
    set_tint({1, 1, 1, 1});
    setUniform(OffsetUniform, Vector2(_camera2)*.5f);
    setUniform(EnableLightmapUniform, _enable_lightmap);
    setUniform(SamplerUniform, _real_sampler = _sampler);
    setUniform(LightmapSamplerUniform, 1);
}

tile_shader::~tile_shader() = default;

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    if (scale != _scale)
        setUniform(ScaleUniform, 2.f/(_scale = scale));
    return *this;
}

tile_shader& tile_shader::set_camera2(Vector2i camera2)
{
    _camera2 = camera2;
    return *this;
}

tile_shader& tile_shader::set_tint(const Vector4& tint)
{
    _tint = tint;
    return *this;
}

tile_shader& tile_shader::set_lightmap_enabled(bool value)
{
    if (value != _enable_lightmap)
        setUniform(EnableLightmapUniform, _enable_lightmap = value);
    return *this;
}

tile_shader& tile_shader::set_sampler(Int sampler)
{
    _sampler = sampler;
    return *this;
}

void tile_shader::draw_pre(GL::AbstractTexture& tex)
{
    fm_assert(Math::abs(_camera2[0]) <= 1 << 25 && Math::abs(_camera2[1]) <= 1 << 25);

    if (_tint != _real_tint)
        setUniform(TintUniform, _real_tint = _tint);

    const auto offset = Vector2(_camera2)*.5f;
    if (offset != _real_camera_offset)
        setUniform(OffsetUniform, _real_camera_offset = offset);

    auto id = tuc.bind(tex);
    set_sampler(id);
    if (_sampler != _real_sampler)
        setUniform(SamplerUniform, _real_sampler = _sampler);
}

void tile_shader::draw_post(GL::AbstractTexture& tex) // NOLINT(*-convert-member-functions-to-static)
{
    (void)tex;
}

Vector2i tile_shader::project2(Vector3i pt)
{
    const auto x = pt[0], y = pt[1], z = pt[2];
    return { 2*(x-y), x+y-2*z };
}

point tile_shader::pixel_to_point(Vector2i pixel, Vector2i window_size, Vector2i camera2, int8_t z_level)
{
    const auto s2 = 2*pixel - window_size - camera2 + Vector2i{0, 2*z_level*tile_size_z};
    // unproject(s2), which is 4× the world position
    const auto w4 = Vector2i{s2.x() + 2*s2.y(), 2*s2.y() - s2.x()};
    const auto p = floor_divmod<4>(w4 + Vector2i{2}).first();
    return point{Vector3i{p, z_level*tile_size_z}};
}

void tile_shader::setUniform(Uniform u, auto value)
{
    fm_assert(u < UNIFORM_COUNT);
    Int loc = uniform_locations[u];
    AbstractShaderProgram::setUniform(loc, value);
}

} // namespace floormat
