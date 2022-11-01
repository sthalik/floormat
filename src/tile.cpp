#include "tile.hpp"
#include "chunk.hpp"

namespace floormat {

pass_mode_ref::pass_mode_ref(chunk& c, std::uint8_t i) noexcept : _chunk{&c}, i{i}
{
}

pass_mode_ref& pass_mode_ref::operator=(pass_mode x) noexcept
{
    auto& bitset = _chunk->_passability;
    bitset[i*2 + 0] = x & 1;
    bitset[i*2 + 1] = x >> 1 & 1;
    return *this;
}

pass_mode_ref& pass_mode_ref::operator=(const pass_mode_ref& x) noexcept
{
    return operator=(pass_mode(x)); // NOLINT(misc-unconventional-assign-operator)
}

pass_mode_ref::operator pass_mode() const noexcept
{
    auto& bitset = _chunk->_passability;
    std::uint8_t ret = 0;
    ret |= (std::uint8_t)bitset[i*2 + 1];
    ret |= (std::uint8_t)bitset[i*2 + 0] << 1;
    return pass_mode(ret);
}

bool operator==(const tile_proto&, const tile_proto&) noexcept = default;

tile_ref::tile_ref(struct chunk& c, std::uint8_t i) noexcept : _chunk{&c}, i{i}
{
}

tile_image_ref tile_ref::ground() noexcept     { return {_chunk->_ground_atlases[i],     _chunk->_ground_variants[i] };     }
tile_image_ref tile_ref::wall_north() noexcept { return {_chunk->_wall_north_atlases[i], _chunk->_wall_north_variants[i] }; }
tile_image_ref tile_ref::wall_west() noexcept  { return {_chunk->_wall_west_atlases[i],  _chunk->_wall_west_variants[i] };  }

tile_image_proto tile_ref::ground() const noexcept     { return {_chunk->_ground_atlases[i],     _chunk->_ground_variants[i] };     }
tile_image_proto tile_ref::wall_north() const noexcept { return {_chunk->_wall_north_atlases[i], _chunk->_wall_north_variants[i] }; }
tile_image_proto tile_ref::wall_west() const noexcept  { return {_chunk->_wall_west_atlases[i],  _chunk->_wall_west_variants[i] };  }

pass_mode_ref tile_ref::pass_mode() noexcept { return { *_chunk, i }; }
pass_mode tile_ref::pass_mode() const noexcept { return pass_mode_ref { *const_cast<chunk*>(_chunk), i }; }

tile_ref::operator tile_proto() const noexcept
{
    return {
        _chunk->_ground_atlases[i], _chunk->_wall_north_atlases[i], _chunk->_wall_west_atlases[i],
        _chunk->_ground_variants[i], _chunk->_wall_north_variants[i], _chunk->_wall_west_variants[i],
        pass_mode(),
    };
}

bool operator==(const tile_ref& a, const tile_ref& b) noexcept
{
    if (a._chunk == b._chunk && a.i == b.i)
        return true;
    else
        return a.ground()      == b.ground() &&
               a.wall_north()  == b.wall_north() &&
               a.wall_west()   == b.wall_west() &&
               a.pass_mode()   == b.pass_mode();
}

} // namespace floormat
