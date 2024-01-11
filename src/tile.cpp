#include "tile.hpp"
#include "chunk.hpp"
#include "src/ground-atlas.hpp"

namespace floormat {

// no avoiding it with rotations
static_assert(iTILE_SIZE2.x() == iTILE_SIZE2.y());

bool operator==(const tile_proto& a, const tile_proto& b) noexcept {
    if (const auto &A = a.ground(), &B = b.ground(); A != B)
        return false;
    if (const auto &A = a.wall_north(), &B = b.wall_north(); A != B)
        return false;
    if (const auto &A = a.wall_west(), &B = b.wall_west(); A != B)
        return false;
    return true;
};

tile_image_proto tile_proto::ground() const noexcept     { return { ground_atlas, ground_variant         }; }
wall_image_proto tile_proto::wall_north() const noexcept { return { wall_north_atlas, wall_north_variant }; }
wall_image_proto tile_proto::wall_west() const noexcept  { return { wall_west_atlas, wall_west_variant   }; }

tile_ref::tile_ref(struct chunk& c, uint8_t i) noexcept : _chunk{&c}, i{i} {}

std::shared_ptr<class ground_atlas> tile_ref::ground_atlas()     noexcept { return _chunk->_ground ? _chunk->_ground->atlases[i] : nullptr; }
std::shared_ptr<class wall_atlas> tile_ref::wall_north_atlas() noexcept { return _chunk->_walls ? _chunk->_walls->atlases[i*2+0] : nullptr; }
std::shared_ptr<class wall_atlas> tile_ref::wall_west_atlas()  noexcept { return _chunk->_walls ? _chunk->_walls->atlases[i*2+1] : nullptr; }

std::shared_ptr<const class ground_atlas> tile_ref::ground_atlas() const noexcept { return _chunk->_ground ? _chunk->_ground->atlases[i] : nullptr; }
std::shared_ptr<const class wall_atlas> tile_ref::wall_north_atlas() const noexcept { return _chunk->_walls ? _chunk->_walls->atlases[i*2+0] : nullptr; }
std::shared_ptr<const class wall_atlas> tile_ref::wall_west_atlas() const noexcept { return _chunk->_walls ? _chunk->_walls->atlases[i*2+1] : nullptr; }

tile_image_ref tile_ref::ground() noexcept     { _chunk->ensure_alloc_ground(); return {_chunk->_ground->atlases[i], _chunk->_ground->variants[i] };     }
wall_image_ref tile_ref::wall_north() noexcept { _chunk->ensure_alloc_walls(); return {_chunk->_walls->atlases[i*2+0], _chunk->_walls->variants[i*2+0] }; }
wall_image_ref tile_ref::wall_west() noexcept  { _chunk->ensure_alloc_walls(); return {_chunk->_walls->atlases[i*2+1],  _chunk->_walls->variants[i*2+1] };  }

tile_image_proto tile_ref::ground() const noexcept
{
    _chunk->ensure_alloc_ground();
    return { _chunk->_ground->atlases[i], _chunk->_ground->variants[i] };
}

wall_image_proto tile_ref::wall_north() const noexcept
{
    if (!_chunk->_walls) [[unlikely]]
        return {};
    else
        return { _chunk->_walls->atlases[i*2+0], _chunk->_walls->variants[i*2+0] };
}

wall_image_proto tile_ref::wall_west() const noexcept
{
    if (!_chunk->_walls) [[unlikely]]
        return {};
    else
        return { _chunk->_walls->atlases[i*2+1],  _chunk->_walls->variants[i*2+1] };
}

tile_ref::operator tile_proto() const noexcept
{
    _chunk->ensure_alloc_ground();
    _chunk->ensure_alloc_walls();
    return {
        _chunk->_ground->atlases[i],  _chunk->_walls->atlases[i*2+0],  _chunk->_walls->atlases[i*2+1],
        _chunk->_ground->variants[i], _chunk->_walls->variants[i*2+0], _chunk->_walls->variants[i*2+1],
    };
}

bool operator==(const tile_ref& a, const tile_ref& b) noexcept
{
    if (a._chunk == b._chunk && a.i == b.i)
        return true;
    else
        return a.ground()     == b.ground() &&
               a.wall_north() == b.wall_north() &&
               a.wall_west()  == b.wall_west();
}

} // namespace floormat
