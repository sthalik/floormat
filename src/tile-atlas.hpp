#pragma once
#include <Corrade/Containers/String.h>
#include <Magnum/Magnum.h>
#include <Magnum/GL/Texture.h>
#include <array>
#include <memory>

namespace std::filesystem { class path; }

namespace floormat {

struct tile_atlas final
{
    using quad = std::array<Vector3, 4>;
    using texcoords = std::array<Vector2, 4>;

    tile_atlas(StringView name, const ImageView2D& img, Vector2ub tile_count);

    texcoords texcoords_for_id(std::size_t id) const;
    static constexpr quad floor_quad(Vector3 center, Vector2 size);
    static constexpr quad wall_quad_N(Vector3 center, Vector3 size);
    static constexpr quad wall_quad_W(Vector3 center, Vector3 size);
    static constexpr std::array<UnsignedShort, 6> indices(std::size_t N);
    [[maybe_unused]] Vector2ui pixel_size() const { return size_; }
    std::size_t num_tiles() const { return Vector2ui{dims_}.product(); }
    Vector2ub num_tiles2() const { return dims_; }
    GL::Texture2D& texture() { return tex_; }
    StringView name() const { return name_; }

private:
    static std::unique_ptr<const texcoords[]> make_texcoords_array(Vector2ui pixel_size, Vector2ub tile_count);
    static texcoords make_texcoords(Vector2ui pixel_size, Vector2ub tile_count, std::size_t i);

    std::unique_ptr<const texcoords[]> texcoords_;
    GL::Texture2D tex_;
    String name_;
    Vector2ui size_;
    Vector2ub dims_;
};

constexpr std::array<UnsignedShort, 6> tile_atlas::indices(std::size_t N)
{
    using u16 = UnsignedShort;
    return {                                        /* 3--1  1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | /  /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/  / | */
    };                                              /* 2  2--0 */
}

constexpr tile_atlas::quad tile_atlas::floor_quad(const Vector3 center, const Vector2 size)
{
    float x = size[0]*.5f, y = size[1]*.5f;
    return {{
        { x + center[0], -y + center[1], center[2]},
        { x + center[0],  y + center[1], center[2]},
        {-x + center[0], -y + center[1], center[2]},
        {-x + center[0],  y + center[1], center[2]},
    }};
}

constexpr tile_atlas::quad tile_atlas::wall_quad_W(const Vector3 center, const Vector3 size)
{
    float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    return {{
        {-x + center[0],  y + center[1],     center[2] },
        {-x + center[0],  y + center[1], z + center[2] },
        {-x + center[0], -y + center[1],     center[2] },
        {-x + center[0], -y + center[1], z + center[2] },
    }};
}

constexpr tile_atlas::quad tile_atlas::wall_quad_N(const Vector3 center, const Vector3 size)
{
    float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    return {{
        { x + center[0], -y + center[1],     center[2] },
        { x + center[0], -y + center[1], z + center[2] },
        {-x + center[0], -y + center[1],     center[2] },
        {-x + center[0], -y + center[1], z + center[2] },
    }};
}

} // namespace floormat
