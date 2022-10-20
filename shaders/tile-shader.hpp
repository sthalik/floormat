#pragma once
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

namespace floormat {

struct tile_shader : GL::AbstractShaderProgram
{
    typedef GL::Attribute<0, Vector3> Position;
    typedef GL::Attribute<1, Vector2> TextureCoordinates;

    explicit tile_shader();
    ~tile_shader() override;

    Vector2 scale() const { return _scale; }
    tile_shader& set_scale(const Vector2& scale);
    Vector2d camera_offset() const { return _camera_offset; }
    tile_shader& set_camera_offset(Vector2d camera_offset);
    Vector4 tint() const { return _tint; }
    tile_shader& set_tint(const Vector4& tint);

    static constexpr Vector2d project(Vector3d pt);
    static constexpr Vector2d unproject(Vector2d px);

    template<typename T, typename... Xs>
    auto draw(T&& mesh, Xs&&... xs) ->
        decltype(GL::AbstractShaderProgram::draw(std::forward<T>(mesh), std::forward<Xs>(xs)...));

private:
    void _draw();

    Vector2d _camera_offset;
    Vector4 _tint;
    Vector2 _scale;
    Vector2 _real_camera_offset;

    enum { ScaleUniform = 0, OffsetUniform = 1, TintUniform = 2, };
};

template<typename T, typename... Xs>
auto tile_shader::draw(T&& mesh, Xs&&... xs) ->
    decltype(GL::AbstractShaderProgram::draw(std::forward<T>(mesh), std::forward<Xs>(xs)...))
{
    _draw();
    return GL::AbstractShaderProgram::draw(std::forward<T>(mesh), std::forward<Xs>(xs)...);
}

constexpr Vector2d tile_shader::project(const Vector3d pt)
{
    const auto x = pt[0], y = pt[1], z = pt[2];
    return { (x-y), (x+y+z*2)*.59 };
}

constexpr Vector2d tile_shader::unproject(const Vector2d px)
{
    const auto X = px[0], Y = px[1];
    return { X + 100 * Y / 59, 100 * Y / 59 - X };
}

} // namespace floormat
