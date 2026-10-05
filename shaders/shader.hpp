#pragma once
#include <cr/StringView.h>
#include <mg/AbstractShaderProgram.h>
#include <mg/Vector4.h>

namespace Magnum::GL { class AbstractTexture; }

namespace floormat {

struct texture_unit_cache;
struct local_coords;
struct point;

struct tile_shader final : private GL::AbstractShaderProgram
{
    using Position           = GL::Attribute<0, Vector3>;
    using TextureCoordinates = GL::Attribute<1, Vector3>;
    using LightCoord         = GL::Attribute<2, Vector2>;
    using Depth              = GL::Attribute<3, float>;

    explicit tile_shader(texture_unit_cache& tuc);
    ~tile_shader() override;

    Vector2 scale() const { return _scale; }
    tile_shader& set_scale(const Vector2& scale);
    Vector2d camera_offset() const { return _camera_offset; }
    /// floor(camera_offset()). Its fraction stays in camera_offset().
    Vector2i camera_offsetʹ() const { return _camera_offsetʹ; }
    tile_shader& set_camera_offset(const Vector2d& camera_offset);
    Vector4 tint() const { return _tint; }
    tile_shader& set_tint(const Vector4& tint);
    bool is_lightmap_enabled() const { return _enable_lightmap; }
    tile_shader& set_lightmap(GL::AbstractTexture* tex);

    template<typename T = float> static constexpr Math::Vector2<T> project(const Math::Vector3<T>& pt);
    template<typename T = float> static constexpr Math::Vector2<T> unproject(const Math::Vector2<T>& px);
    /// project(), doubled: y is a half pixel when x+y is odd.
    static Vector2i projectʹ(Vector3i pt);
    /// Nearest point on z_level's floor to the pixel, halves rounded up. A point whose y
    /// is a half pixel is picked from the pixel above it. camera is camera_offsetʹ().
    static point pixel_to_point(Vector2i pixel, Vector2i window_size, Vector2i camera, int8_t z_level);
    /// window_size/2 + camera + project(world), exact. y ends in .5 when x+y is odd.
    static Vector2 point_to_pixel(Vector3i world, Vector2i window_size, Vector2i camera);
    static Vector2 point_to_pixel(Vector3 world, Vector2i window_size, Vector2i camera);
    /// point_to_pixel() with a half-pixel y moved up: the pixel pixel_to_point() maps back to world.
    static Vector2i point_to_pixelʹ(Vector3i world, Vector2i window_size, Vector2i camera);

    template<typename T, typename... Xs> GL::AbstractShaderProgram& draw(GL::AbstractTexture& tex, T&& mesh, Xs&&... xs);

    static constexpr float foreshortening_factor = float{0.5};

    tile_shader& set_sampler(Int sampler);
    Int sampler() const { return _sampler; }

private:
    void draw_pre(GL::AbstractTexture& tex);
    void draw_post(GL::AbstractTexture& tex);

    enum Uniform : uint8_t {
        ScaleUniform = 0, OffsetUniform = 1, TintUniform = 2,
        EnableLightmapUniform = 3,
        SamplerUniform = 4, LightmapSamplerUniform = 5,
        UNIFORM_COUNT = 6
    };

    void setUniform(Uniform u, auto value);

    Int uniform_locations[UNIFORM_COUNT] = {};

    static constexpr StringView uniform_names[UNIFORM_COUNT] = {
        "scale"_s,
        "offset"_s,
        "tint"_s,
        "enable_lightmap"_s,
        "sampler"_s,
        "lightmap_sampler"_s,
    };

    static constexpr StringView attribute_names[] = {
        "position"_s,
        "texcoords"_s,
        "light_coord"_s,
        "depth"_s,
    };

    texture_unit_cache& tuc; // NOLINT(*-avoid-const-or-ref-data-members)
    GL::AbstractTexture* _lightmap = nullptr;
    Vector2d _camera_offset;
    Vector2i _camera_offsetʹ;
    Vector4 _tint, _real_tint;
    Vector2 _scale;
    Vector2i _real_camera_offsetʹ;
    bool _enable_lightmap : 1 = false;
    Int _sampler = 0, _real_sampler, _real_lightmap_sampler;
};

template<typename T, typename... Xs>
GL::AbstractShaderProgram& tile_shader::draw(GL::AbstractTexture& tex, T&& mesh, Xs&&... xs)
{
    draw_pre(tex);
    decltype(auto) ret = GL::AbstractShaderProgram::draw(forward<T>(mesh), forward<Xs>(xs)...);
    draw_post(tex);
    return ret;
}

template<typename T>
constexpr Math::Vector2<T> tile_shader::project(const Math::Vector3<T>& pt)
{
    static_assert(std::is_floating_point_v<T>);
    const auto x = pt[0], y = pt[1], z = -pt[2];
    return { x-y, (x+y+z*2)*T(foreshortening_factor) };
}

template<typename T>
constexpr Math::Vector2<T> tile_shader::unproject(const Math::Vector2<T>& px)
{
    static_assert(std::is_floating_point_v<T>);
    const auto X = px[0], Y = px[1];
    const auto Yʹ = Y / T(foreshortening_factor);
    return { X + Yʹ, Yʹ - X };
}

} // namespace floormat
