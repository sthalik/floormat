#pragma once
#include "compat/defs.hpp"
#include "tile-defs.hpp"
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

namespace floormat {

struct local_coords;

struct tile_shader : GL::AbstractShaderProgram
{
    using Position           = GL::Attribute<0, Vector3>;
    using TextureCoordinates = GL::Attribute<1, Vector2>;
    using Depth              = GL::Attribute<2, float>;

    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(tile_shader);
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(tile_shader);

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

    static constexpr Vector2s max_screen_tiles{16, 16};
    static constexpr float depth_tile_size = 1/(double)(TILE_COUNT * max_screen_tiles.product());
    static constexpr float scenery_depth_offset = 0.25f, character_depth_offset = 0.25f, wall_depth_offset = 0.125f;

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
    return GL::AbstractShaderProgram::draw(std::forward<T>(mesh), std::forward<Xs>(xs)...);
}

template<typename T>
constexpr Math::Vector2<T> tile_shader::project(const Math::Vector3<T>& pt)
{
    static_assert(std::is_floating_point_v<T>);
    const auto x = pt[0], y = pt[1], z = -pt[2];
    return { x-y, (x+y+z*2)*T(.59) };
}

template<typename T>
constexpr Math::Vector2<T> tile_shader::unproject(const Math::Vector2<T>& px)
{
    const auto X = px[0], Y = px[1];
    return { X + 100 * Y / 59, 100 * Y / 59 - X };
}

} // namespace floormat
