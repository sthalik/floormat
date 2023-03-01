#include "tile.hpp"
#include "chunk.hpp"

namespace floormat {

bool operator==(const tile_proto& a, const tile_proto& b) noexcept {
    return a.ground()     == b.ground() &&
           a.wall_north() == b.wall_north() &&
           a.wall_west()  == b.wall_west() &&
           a.scenery()    == b.scenery();
};

tile_image_proto tile_proto::ground() const noexcept     { return { ground_atlas, ground_variant         }; }
tile_image_proto tile_proto::wall_north() const noexcept { return { wall_north_atlas, wall_north_variant }; }
tile_image_proto tile_proto::wall_west() const noexcept  { return { wall_west_atlas, wall_west_variant   }; }
scenery_proto tile_proto::scenery() const noexcept       { return { scenery_atlas, scenery_frame         }; }

tile_ref::tile_ref(struct chunk& c, std::uint8_t i) noexcept : _chunk{&c}, i{i} {}

std::shared_ptr<tile_atlas> tile_ref::ground_atlas()     noexcept { return _chunk->_ground_atlases[i]; }
std::shared_ptr<tile_atlas> tile_ref::wall_north_atlas() noexcept { return _chunk->_wall_atlases[i*2+0]; }
std::shared_ptr<tile_atlas> tile_ref::wall_west_atlas()  noexcept { return _chunk->_wall_atlases[i*2+1]; }
std::shared_ptr<anim_atlas> tile_ref::scenery_atlas()    noexcept { return _chunk->_scenery_atlases[i]; }

std::shared_ptr<const tile_atlas> tile_ref::ground_atlas()      const noexcept { return _chunk->_ground_atlases[i]; }
std::shared_ptr<const tile_atlas> tile_ref::wall_north_atlas()  const noexcept { return _chunk->_wall_atlases[i*2+0]; }
std::shared_ptr<const tile_atlas> tile_ref::wall_west_atlas()   const noexcept { return _chunk->_wall_atlases[i*2+1]; }
std::shared_ptr<const anim_atlas> tile_ref::scenery_atlas()     const noexcept { return _chunk->_scenery_atlases[i]; }

tile_image_ref tile_ref::ground() noexcept     { return {_chunk->_ground_atlases[i],     _chunk->_ground_variants[i] };     }
tile_image_ref tile_ref::wall_north() noexcept { return {_chunk->_wall_atlases[i*2+0], _chunk->_wall_variants[i*2+0] }; }
tile_image_ref tile_ref::wall_west() noexcept  { return {_chunk->_wall_atlases[i*2+1],  _chunk->_wall_variants[i*2+1] };  }
scenery_ref tile_ref::scenery() noexcept       { return { *_chunk, i }; }

tile_image_proto tile_ref::ground() const noexcept     { return { _chunk->_ground_atlases[i],     _chunk->_ground_variants[i] };     }
tile_image_proto tile_ref::wall_north() const noexcept { return { _chunk->_wall_atlases[i*2+0], _chunk->_wall_variants[i*2+0] }; }
tile_image_proto tile_ref::wall_west() const noexcept  { return { _chunk->_wall_atlases[i*2+1],  _chunk->_wall_variants[i*2+1] };  }
scenery_proto tile_ref::scenery() const noexcept       { return { _chunk->_scenery_atlases[i],    _chunk->_scenery_variants[i] }; }

tile_ref::operator tile_proto() const noexcept
{
    return {
        _chunk->_ground_atlases[i],  _chunk->_wall_atlases[i*2+0],  _chunk->_wall_atlases[i*2+1], _chunk->_scenery_atlases[i],
        _chunk->_ground_variants[i], _chunk->_wall_variants[i*2+0], _chunk->_wall_variants[i*2+1], _chunk->_scenery_variants[i],
    };
}

bool operator==(const tile_ref& a, const tile_ref& b) noexcept
{
    if (a._chunk == b._chunk && a.i == b.i)
        return true;
    else
        return a.ground()     == b.ground() &&
               a.wall_north() == b.wall_north() &&
               a.wall_west()  == b.wall_west() &&
               a.scenery()    == b.scenery();
}

} // namespace floormat
