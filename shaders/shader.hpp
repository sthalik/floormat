#pragma once
#include "tile-defs.hpp"
#include <Corrade/Utility/Move.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

namespace floormat {

struct local_coords;

struct tile_shader final : private GL::AbstractShaderProgram
{
    using Position           = GL::Attribute<0, Vector3>;
    using TextureCoordinates = GL::Attribute<1, Vector2>;
    using Depth              = GL::Attribute<2, float>;

    explicit tile_shader();
    ~tile_shader() override;

    Vector2 scale() const { return _scale; }
    tile_shader& set_scale(const Vector2& scale);
    Vector2d camera_offset() const { return _camera_offset; }
    tile_shader& set_camera_offset(const Vector2d& camera_offset, float depth_offset);
    Vector4 tint() const { return _tint; }
    tile_shader& set_tint(const Vector4& tint);
    float depth_offset() const { return _depth_offset; }
    static float depth_value(const local_coords& xy, float offset = 0) noexcept;

    template<typename T = float> static constexpr Math::Vector2<T> project(const Math::Vector3<T>& pt);
    template<typename T = float> static constexpr Math::Vector2<T> unproject(const Math::Vector2<T>& px);

    template<typename T, typename... Xs>
    decltype(auto) draw(T&& mesh, Xs&&... xs);

    static constexpr Vector2s max_screen_tiles = {8, 8};
    static constexpr float character_depth_offset = 1 + 1./64;
    static constexpr float scenery_depth_offset = 1 + 1./64;
    static constexpr float ground_depth_offset = 0; // todo scenery cut off at chunk boundary
    static constexpr float wall_depth_offset = 1;
    static constexpr float z_depth_offset = 1 + 2./64;
    static constexpr float depth_tile_size = 1/(double)(TILE_MAX_DIM * 2 * max_screen_tiles.product());

private:
    void _draw();

    Vector2d _camera_offset;
    Vector4 _tint, _real_tint;
    Vector2 _scale;
    Vector3 _real_camera_offset;
    float _depth_offset = 0;

    enum { ScaleUniform = 0, OffsetUniform = 1, TintUniform = 2, };
};

template<typename T, typename... Xs>
decltype(auto) tile_shader::draw(T&& mesh, Xs&&... xs)
{
    _draw();
    return GL::AbstractShaderProgram::draw(Utility::forward<T>(mesh), Utility::forward<Xs>(xs)...);
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
