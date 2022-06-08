#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/GL/Texture.h>
#include <array>

namespace Magnum::Examples {

struct atlas_texture final
{
    using vertex_array_type = std::array<Vector3, 4>;

    atlas_texture(const ImageView2D& img, Vector2i dims);
    std::array<Vector2, 4> texcoords_for_id(int id) const;
    static vertex_array_type floor_quad(Vector3 center, Vector2 size);
    static vertex_array_type wall_quad_W(Vector3 center, Vector3 size);
    static vertex_array_type wall_quad_S(Vector3 center, Vector3 size);
    static vertex_array_type wall_quad_E(Vector3 center, Vector3 size);
    static vertex_array_type wall_quad_N(Vector3 center, Vector3 size);
    static std::array<UnsignedShort, 6> indices(int N);
    GL::Texture2D& texture() { return tex_; }
    constexpr int size() const { return dims_.product(); }
    constexpr Vector2i tile_size() const { return tile_size_; }

    atlas_texture(const atlas_texture&) = delete;
    atlas_texture& operator=(const atlas_texture&) = delete;
private:
    GL::Texture2D tex_;
    Vector2i size_, dims_, tile_size_;
};

} // namespace Magnum::Examples
