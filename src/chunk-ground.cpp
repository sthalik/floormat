#include "chunk.hpp"
#include "tile-constants.hpp"
#include "ground-atlas.hpp"
#include "quads.hpp"
#include "spritebatch.hpp"
#include "point.inl"
#include "depth.hpp"
#include "compat/borrowed-ptr.hpp"
#include "compat/map.hpp"

namespace floormat {

void chunk::ensure_alloc_ground()
{
    if (!_ground) [[unlikely]]
        _ground = Pointer<ground_stuff>{InPlaceInit};
}

void chunk::ensure_ground_mesh(SpriteBatch& sb)
{
    if (!_ground)
        return;
    if (_ground_modified)
    {
        _ground_modified = false;
        ground_static_mesh.clear();
        ground_static_mesh.reserve(TILE_COUNT);

        const float depth = Depth::value_at(sb.depth_start(), point{_coord, {}, {}}, -tile_size_xy * 4);

        for (auto i = 0uz; i < TILE_COUNT; i++)
        {
            const auto& atlas = _ground->atlases[i];
            if (!atlas)
                continue;
            const local_coords pos{(u8)i};
            const auto center = Vector3(point{_coord, pos, {}});
            const auto quad = Quads::floor_quad(center, TILE_SIZE2);
            const auto texcoords = atlas->texcoords_for_id(_ground->variants[i] % atlas->num_tiles());
            const auto v = map([&](u8 k) -> Quads::vertex {
                return { quad[k], texcoords[k], depth };
            }, Quads::ccw_order);
            ground_static_mesh.add(v, depth);
        }
    }
    sb.emit(ground_static_mesh);
}

} // namespace floormat
