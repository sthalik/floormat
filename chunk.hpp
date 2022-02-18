#pragma once
#include <Magnum/Math/Vector.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Trade/ImageData.h>
#include <array>

namespace Magnum::Examples {
using Vector2i = Math::Vector<2, int>;

struct atlas_texture final
{
    atlas_texture(const Trade::ImageData2D& img, Vector2i dims);
    std::array<Vector2, 4> texcoords_for_id(int id) const;
    static std::array<Vector3, 4> floor_quad(Vector3 center, Vector2 size);
    static std::array<UnsignedShort, 6> indices(int N);
    GL::Texture2D& texture() { return tex_; }

    atlas_texture(const atlas_texture&) = delete;
    atlas_texture& operator=(const atlas_texture&) = delete;
private:
    GL::Texture2D tex_;
    Vector2i size_, dims_, tile_size_;
};

} // namespace Magnum::Examples
