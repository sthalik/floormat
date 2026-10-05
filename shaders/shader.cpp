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
    setUniform(OffsetUniform, Vector2(_real_camera_offsetʹ)*.5f);
    setUniform(EnableLightmapUniform, _enable_lightmap);
    setUniform(SamplerUniform, _real_sampler = _sampler);
    setUniform(LightmapSamplerUniform, _real_lightmap_sampler = 0);
}

tile_shader::~tile_shader() = default;

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    if (scale != _scale)
        setUniform(ScaleUniform, 2.f/(_scale = scale));
    return *this;
}

tile_shader& tile_shader::set_camera_offset(const Vector2d& camera_offset)
{
    _camera_offset = camera_offset;
    // floor(c + k) == floor(c) + k, which get_draw_bounds() needs to predict with_shifted_camera_offset.
    _camera_offsetʹ = Vector2i(Math::floor(camera_offset));
    return *this;
}

tile_shader& tile_shader::set_tint(const Vector4& tint)
{
    _tint = tint;
    return *this;
}

tile_shader& tile_shader::set_lightmap(GL::AbstractTexture* tex)
{
    _lightmap = tex;
    if ((tex != nullptr) != _enable_lightmap)
        setUniform(EnableLightmapUniform, _enable_lightmap = tex != nullptr);
    return *this;
}

tile_shader& tile_shader::set_sampler(Int sampler)
{
    _sampler = sampler;
    return *this;
}

void tile_shader::draw_pre(GL::AbstractTexture& tex)
{
    if (_tint != _real_tint)
        setUniform(TintUniform, _real_tint = _tint);

    // GL's origin is win*.5 but the mappings use win/2, half a pixel apart on an odd axis.
    // Doubled to keep that half an integer. Floats hold every integer up to 2^24.
    const auto win = Vector2i(_scale);
    const auto offsetʹ = 2*(win/2 + _camera_offsetʹ) - win;
    fm_assert((Math::abs(offsetʹ) <= Vector2i{1 << 24}).all());
    if (offsetʹ != _real_camera_offsetʹ)
        setUniform(OffsetUniform, Vector2(_real_camera_offsetʹ = offsetʹ)*.5f);

    auto id = tuc.bind(tex);
    set_sampler(id);
    if (_sampler != _real_sampler)
        setUniform(SamplerUniform, _real_sampler = _sampler);

    // GL rejects a draw with two sampler types on one unit. The cache
    // never hands out unit 0, so the atlas can't be there.
    const auto lightmap_id = _lightmap ? tuc.bind(_lightmap) : 0;
    if (lightmap_id != _real_lightmap_sampler)
        setUniform(LightmapSamplerUniform, _real_lightmap_sampler = lightmap_id);
}

void tile_shader::draw_post(GL::AbstractTexture& tex) // NOLINT(*-convert-member-functions-to-static)
{
    (void)tex;
}

Vector2i tile_shader::projectʹ(Vector3i pt)
{
    const auto x = pt[0], y = pt[1], z = pt[2];
    return { 2*(x-y), x+y-2*z };
}

point tile_shader::pixel_to_point(Vector2i pixel, Vector2i window_size, Vector2i camera, int8_t z_level)
{
    const auto sʹ = 2*(pixel - window_size/2 - camera) + Vector2i{0, 2*z_level*tile_size_z};
    // unproject(sʹ), which is 4× the world position
    const auto w4 = Vector2i{sʹ.x() + 2*sʹ.y(), 2*sʹ.y() - sʹ.x()};
    const auto p = floor_divmod<4>(w4 + Vector2i{2}).first();
    return point{Vector3i{p, z_level*tile_size_z}};
}

Vector2 tile_shader::point_to_pixel(Vector3i world, Vector2i window_size, Vector2i camera)
{
    return Vector2(projectʹ(world) + 2*(window_size/2 + camera))*.5f;
}

Vector2 tile_shader::point_to_pixel(Vector3 world, Vector2i window_size, Vector2i camera)
{
    return Vector2(window_size/2 + camera) + project(world);
}

Vector2i tile_shader::point_to_pixelʹ(Vector3i world, Vector2i window_size, Vector2i camera)
{
    return window_size/2 + camera + floor_divmod<2>(projectʹ(world)).first();
}

void tile_shader::setUniform(Uniform u, auto value)
{
    fm_assert(u < UNIFORM_COUNT);
    Int loc = uniform_locations[u];
    AbstractShaderProgram::setUniform(loc, value);
}

} // namespace floormat
