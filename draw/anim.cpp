#include "anim.hpp"
#include "anim-atlas.hpp"
#include "shaders/tile.hpp"

namespace floormat {

anim_mesh::anim_mesh() = default;

std::array<UnsignedShort, 6> anim_mesh::make_index_array()
{
    using u16 = std::uint16_t;
    return {{
        (u16)0, (u16)1, (u16)2,
        (u16)2, (u16)1, (u16)3,
    }};
}

void anim_mesh::draw(local_coords xy, const anim_atlas& atlas, const anim_frame& frame)
{
    const auto center_ = Vector3(xy.x, xy.y, 0.f) * TILE_SIZE;
    const auto pos = atlas.frame_quad(center_, frame);
    _positions_buffer.setSubData(0, pos);
}

} // namespace floormat
