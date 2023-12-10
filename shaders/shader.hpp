#pragma once
#include "src/tile-defs.hpp"
#include "shaders/texture-unit-cache.hpp"
#include <utility>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

namespace Magnum::GL { class AbstractTexture; }

namespace floormat {

struct local_coords;

struct tile_shader final : private GL::AbstractShaderProgram
{
    using Position           = GL::Attribute<0, Vector3>;
    using TextureCoordinates = GL::Attribute<1, Vector2>;
    using LightCoord         = GL::Attribute<2, Vector2>;
    using Depth              = GL::Attribute<3, float>;

    explicit tile_shader(texture_unit_cache& tuc);
    ~tile_shader() override;

    Vector2 scale() const { return _scale; }
    tile_shader& set_scale(const Vector2& scale);
    Vector2d camera_offset() const { return _camera_offset; }
    tile_shader& set_camera_offset(const Vector2d& camera_offset, float depth_offset);
    Vector4 tint() const { return _tint; }
    tile_shader& set_tint(const Vector4& tint);
    float depth_offset() const { return _depth_offset; }
    static float depth_value(const local_coords& xy, float offset = 0) noexcept;
    bool is_lightmap_enabled() const { return _enable_lightmap; }
    tile_shader& set_lightmap_enabled(bool value);

    template<typename T = float> static constexpr Math::Vector2<T> project(const Math::Vector3<T>& pt);
    template<typename T = float> static constexpr Math::Vector2<T> unproject(const Math::Vector2<T>& px);

    template<typename T, typename... Xs> decltype(auto) draw(GL::AbstractTexture& tex, T&& mesh, Xs&&... xs);

    static constexpr Vector2s max_screen_tiles = {8, 8};
    static constexpr float character_depth_offset = 1 + 2./64;
    static constexpr float scenery_depth_offset = 1 + 2./64;
    static constexpr float ground_depth_offset = 0;
    static constexpr float wall_depth_offset = 1;
    static constexpr float wall_overlay_depth_offset = 1 + 1./64;
    static constexpr float z_depth_offset = 1 + 4./64;
    static constexpr float depth_tile_size = 1.f/(TILE_MAX_DIM * 2 * max_screen_tiles.product());
    static constexpr float foreshortening_factor = 0.578125f;

    tile_shader& set_sampler(Int sampler);
    Int sampler() const { return _sampler; }

private:
    void draw_pre(GL::AbstractTexture& tex);
    void draw_post(GL::AbstractTexture& tex);

    texture_unit_cache& tuc; // NOLINT(*-avoid-const-or-ref-data-members)
    Vector2d _camera_offset;
    Vector4 _tint, _real_tint;
    Vector2 _scale;
    Vector3 _real_camera_offset;
    float _depth_offset = 0;
    bool _enable_lightmap : 1 = false;
    Int _sampler = 0, _real_sampler;

    enum {
        ScaleUniform = 0, OffsetUniform = 1, TintUniform = 2,
        EnableLightmapUniform = 3,
        SamplerUniform = 4, LightmapSamplerUniform = 5,
    };
};

template<typename T, typename... Xs>
decltype(auto) tile_shader::draw(GL::AbstractTexture& tex, T&& mesh, Xs&&... xs)
{
    draw_pre(tex);
    decltype(auto) ret = GL::AbstractShaderProgram::draw(std::forward<T>(mesh), std::forward<Xs>(xs)...);
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
    const auto Y_ = Y / T(foreshortening_factor);
    return { X + Y_, Y_ - X };
}

} // namespace floormat
