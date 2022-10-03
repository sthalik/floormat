#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/GL/Texture.h>
#include <array>

namespace Magnum::Examples {

struct tile_atlas final
{
    using vertex_array_type = std::array<Vector3, 4>;

    tile_atlas(const ImageView2D& img, Vector2i dims);
    std::array<Vector2, 4> texcoords_for_id(std::size_t id) const;
    static vertex_array_type floor_quad(Vector3 center, Vector2 size);
    static vertex_array_type wall_quad_S(Vector3 center, Vector3 size);
    static vertex_array_type wall_quad_E(Vector3 center, Vector3 size);
    static vertex_array_type wall_quad_N(Vector3 center, Vector3 size);
    static vertex_array_type wall_quad_W(Vector3 center, Vector3 size);
    static constexpr std::array<UnsignedShort, 6> indices(std::size_t N);
    std::size_t size() const { return (std::size_t)dims_.product(); }
    Vector2i tile_size() const { return size_ / dims_; }
    GL::Texture2D& texture() { return tex_; }

    tile_atlas() = default;
    tile_atlas(const tile_atlas&) = delete;
    tile_atlas& operator=(const tile_atlas&) = delete;
private:
    GL::Texture2D tex_;
    Vector2i size_, dims_;
};

constexpr std::array<UnsignedShort, 6> tile_atlas::indices(std::size_t N)
{
    using u16 = UnsignedShort;
    return {                                        /* 3--1  1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | /  /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/  / | */
    };                                              /* 2  2--0 */
}

} // namespace Magnum::Examples
