#include "path-search.hpp"
#include "global-coords.hpp"
#include "object.hpp"
#include "world.hpp"
#include "RTree-search.hpp"
#include <bit>
#include <algorithm>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/PairStl.h>
#include <Magnum/Math/Range.h>

namespace floormat {

namespace {

constexpr inline auto null_coord = global_coords{0, 0, nullptr};
constexpr inline size_t path_size_min = 32;

} // namespace

search_result::~search_result() = default;

void search::ensure_allocated(chunk_coords a, chunk_coords b)
{
    auto new_size = Math::abs(a - b) + Vector2i(3);
    auto new_start = Vector2i(std::min(a.x, b.x), std::min(a.y, b.y)) - Vector2i(1);
    auto size1 = new_size.product();
    fm_debug_assert(size1 > 0);
    cache.start = new_start;
    if ((size_t)size1 > cache.array.size())
    {
        cache.array = Array<chunk_tiles_cache>{ValueInit, (size_t)size1};
        cache.size = new_size;
    }
    else
        for (auto& x : cache.array)
            x = {};
}

bool search::sample_rtree_1(chunk& c, Vector2 min, Vector2 max, object_id own_id)
{
    auto& rt = *c.rtree();
    bool is_passable = true;
    rt.Search(min.data(), max.data(), [&](uint64_t data, const auto&) {
        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
        if (x.data != own_id && x.pass != (uint64_t)pass_mode::pass)
        {
            is_passable = false;
            return false;
        }
        else
            return true;
    });
    return is_passable;
}

bool search::sample_rtree(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id)
{
    if (auto* c = w.at(ch0))
        // it's not correct to return true if c == nullptr
        // because neighbors can still contain bounding boxes for that tile
        if (!sample_rtree_1(*c, min, max, own_id))
            return false;

    for (auto nb : world::neighbor_offsets)
    {
        static_assert(iTILE_SIZE2.x() == iTILE_SIZE2.y());
        constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
        constexpr int bbox_max = 1 << sizeof(Vector2b().x())*8;
        constexpr auto chunk_max = chunk_size + Vector2i(bbox_max);

        const auto off = Vector2(nb)*Vector2(chunk_size);
        const auto min_ = min - off, max_ = max - off;

        if (min_.x() > chunk_max.x() || min_.y() > chunk_max.y())
            continue;
        if (max_.x() < -bbox_max || max_.y() < -bbox_max)
            continue;

        if (auto* c2 = w.at(ch0 + Vector2i(nb)))
            if (!sample_rtree_1(*c2, min_, max_, own_id))
                return false;
    }
    return true;
}

bool search::sample_rtree(world& w, global_coords coord, Vector2b offset, Vector2ub size_, object_id own_id)
{
    auto center = iTILE_SIZE2 * Vector2i(coord.local()) + Vector2i(offset);
    auto size = Vector2(size_);
    auto min = Vector2(center) - size*.5f, max = min + size;
    return sample_rtree(w, coord, min, max, own_id);
}

auto search::make_neighbor_tile_bbox(Vector2i coord, Vector2ub own_size, rotation r) -> bbox
{
    constexpr auto tx = iTILE_SIZE2.x()/2, ty = iTILE_SIZE2.y()/2;

    const auto s  = Math::max(Vector2ui(own_size), Vector2ui(iTILE_SIZE2));
    const auto sx = s.x(), sy = s.y();

    Vector2i off;
    Vector2ui size;

    switch (r)
    {
    case rotation::E: off = { 1,  0}; size = {tx, sy}; break;
    case rotation::N: off = { 0, -1}; size = {sx, ty}; break;
    case rotation::S: off = { 0,  1}; size = {sx, ty}; break;
    case rotation::W: off = {-1,  0}; size = {tx, sy}; break;
    default: fm_abort("wrong 4-way rotation enum value '%d", (int)r);
    }

    auto center1 = coord * iTILE_SIZE2;
    auto center2 = center1 + off*iTILE_SIZE2;
    auto center  = (center1 + center2)/2;

    auto c = Vector2(center), sz = Vector2(size);
    return { c - sz*.5f, c + sz };
}

Optional<search_result> search::operator()(world& w, object_id own_id,
                                           global_coords from, Vector2b from_offset, Vector2ub size,
                                           global_coords to, Vector2b to_offset)
{
    if (from.z() != to.z()) [[unlikely]]
        return {};
    // todo try removing this eventually
    if (from.z() != 0) [[unlikely]]
        return {};

    // check if obj can actually move at all
    if (!sample_rtree(w, from, from_offset, size, own_id))
        return {};

    ensure_allocated(from.chunk(), to.chunk());

    //...
}

Optional<search_result> search::operator()(world& w, const object& obj, global_coords to, Vector2b to_offset)
{
    constexpr auto full_tile = Vector2ub(iTILE_SIZE2*3/4);
    auto size = Math::max(obj.bbox_size, full_tile);

    // todo fixme
    // if bbox_offset is added to obj's offset, then all coordinates in the paths are shifted by bbox_offset.
    // maybe add handling to subtract bbox_offset from the returned path.
    // for that it needs to be passed into callees separately.
    fm_assert(obj.bbox_offset.isZero());
    return operator()(w, obj.id, obj.coord, obj.offset, size, to, to_offset);
}

} // namespace floormat
