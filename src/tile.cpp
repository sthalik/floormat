#include "tile.hpp"
#include "tile-constants.hpp"
#include "chunk.hpp"

namespace floormat {

namespace {

template<typename Ptr, typename Atlas>
Ptr atlas_as(const bptr<Atlas>& a) noexcept
{
    if constexpr(std::is_pointer_v<Ptr>)
        return a.get();
    else
        return a;
}

} // namespace

static_assert(iTILE_SIZE2 == Vector2i{tile_size_xy});
static_assert(iTILE_SIZE == Vector3i{tile_size_xy, tile_size_xy, tile_size_z});
static_assert(iTILE_SIZE2.x() == iTILE_SIZE2.y()); // no avoiding it with rotations
static_assert(tile_size_xy % 2 == 0);

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

template<typename Chunk>
tile_ref_<Chunk>::tile_ref_(Chunk& c, uint8_t i) noexcept : _chunk{&c}, i{i} {}

template<typename Chunk>
tile_image_ref tile_ref_<Chunk>::ground() noexcept requires(!std::is_const_v<Chunk>)
{
    _chunk->ensure_alloc_ground();
    return {_chunk->_ground->atlases[i], _chunk->_ground->variants[i]};
}

template<typename Chunk>
wall_image_ref tile_ref_<Chunk>::wall_north() noexcept requires(!std::is_const_v<Chunk>)
{
    _chunk->ensure_alloc_walls();
    return {_chunk->_walls->atlases[i*2+0], _chunk->_walls->variants[i*2+0]};
}

template<typename Chunk>
wall_image_ref tile_ref_<Chunk>::wall_west() noexcept requires(!std::is_const_v<Chunk>)
{
    _chunk->ensure_alloc_walls();
    return {_chunk->_walls->atlases[i*2+1], _chunk->_walls->variants[i*2+1]};
}

template<typename Chunk>
auto tile_ref_<Chunk>::ground() const noexcept -> ground_image
{
    if (!_chunk->_ground) [[unlikely]]
        return {};
    return { atlas_as<atlas_ptr<class ground_atlas>>(_chunk->_ground->atlases[i]), _chunk->_ground->variants[i] };
}

template<typename Chunk>
auto tile_ref_<Chunk>::wall_north() const noexcept -> wall_image
{
    if (!_chunk->_walls) [[unlikely]]
        return {};
    return { atlas_as<atlas_ptr<class wall_atlas>>(_chunk->_walls->atlases[i*2+0]), _chunk->_walls->variants[i*2+0] };
}

template<typename Chunk>
auto tile_ref_<Chunk>::wall_west() const noexcept -> wall_image
{
    if (!_chunk->_walls) [[unlikely]]
        return {};
    return { atlas_as<atlas_ptr<class wall_atlas>>(_chunk->_walls->atlases[i*2+1]), _chunk->_walls->variants[i*2+1] };
}

template<typename Chunk>
auto tile_ref_<Chunk>::ground_atlas() const noexcept -> atlas_ptr<class ground_atlas>
{
    if (!_chunk->_ground)
        return nullptr;
    return atlas_as<atlas_ptr<class ground_atlas>>(_chunk->_ground->atlases[i]);
}

template<typename Chunk>
auto tile_ref_<Chunk>::wall_north_atlas() const noexcept -> atlas_ptr<class wall_atlas>
{
    if (!_chunk->_walls)
        return nullptr;
    return atlas_as<atlas_ptr<class wall_atlas>>(_chunk->_walls->atlases[i*2+0]);
}

template<typename Chunk>
auto tile_ref_<Chunk>::wall_west_atlas() const noexcept -> atlas_ptr<class wall_atlas>
{
    if (!_chunk->_walls)
        return nullptr;
    return atlas_as<atlas_ptr<class wall_atlas>>(_chunk->_walls->atlases[i*2+1]);
}

template<typename Chunk>
tile_ref_<Chunk>::operator tile_proto() const noexcept
{
    tile_proto p;
    if (_chunk->_ground)
    {
        p.ground_atlas   = _chunk->_ground->atlases[i];
        p.ground_variant = _chunk->_ground->variants[i];
    }
    if (_chunk->_walls)
    {
        p.wall_north_atlas   = _chunk->_walls->atlases[i*2+0];
        p.wall_west_atlas    = _chunk->_walls->atlases[i*2+1];
        p.wall_north_variant = _chunk->_walls->variants[i*2+0];
        p.wall_west_variant  = _chunk->_walls->variants[i*2+1];
    }
    return p;
}

template<typename Chunk>
Chunk& tile_ref_<Chunk>::chunk() const noexcept { return *_chunk; }

template<typename Chunk>
size_t tile_ref_<Chunk>::index() const noexcept { return i; }

template<typename Chunk>
bool operator==(const tile_ref_<Chunk>& a, const tile_ref_<Chunk>& b) noexcept
{
    if (a._chunk == b._chunk && a.i == b.i)
        return true;
    else
        return a.ground()     == b.ground() &&
               a.wall_north() == b.wall_north() &&
               a.wall_west()  == b.wall_west();
}

template struct tile_ref_<chunk>;
template struct tile_ref_<const chunk>;
template bool operator==(const tile_ref_<chunk>&, const tile_ref_<chunk>&) noexcept;
template bool operator==(const tile_ref_<const chunk>&, const tile_ref_<const chunk>&) noexcept;

} // namespace floormat
