#include "path-search-astar.hpp"
#include "compat/int-hash.hpp"
#include <utility>

namespace floormat {

size_t astar_hash::operator()(const astar_edge& e) const
{
    static_assert(sizeof e == 16);
    if constexpr(sizeof(void*) > 4)
        return fnvhash_64(&e, sizeof e);
    else
        return fnvhash_32(&e, sizeof e);
}

bool astar_edge::operator==(const astar_edge&) const noexcept = default;

astar_edge::astar_edge(global_coords coord1, Vector2b off1,
                       global_coords coord2, Vector2b off2) :
    astar_edge {
        chunk_coords_{coord1}, coord1.local(), off1,
        chunk_coords_{coord2}, coord2.local(), off2,
    }
{
}

size_t astar_edge::hash() const
{
    static_assert(sizeof *this == 16);

    if constexpr(sizeof nullptr > 4)
        return fnvhash_64(this, sizeof *this);
    else
        return fnvhash_32(this, sizeof *this);
}

astar_edge astar_edge::swapped() const
{
    auto e = *this;
    std::exchange(e.from_cx,   e.to_cx);
    std::exchange(e.from_cy,   e.to_cy);
    std::exchange(e.from_cz,   e.to_cz);
    std::exchange(e.from_t,    e.to_t);
    std::exchange(e.from_offx, e.to_offx);
    std::exchange(e.from_offy, e.to_offy);
    return e;
}

bool operator<(const astar_edge_tuple& a, const astar_edge_tuple& b)
{
    return b.dist < a.dist;
}

astar::astar()
{
    mins.max_load_factor(0.25f);
    reserve(4096);
}

bool astar::add_visited(const astar_edge& value)
{
    if (seen.insert(value).second)
    {
        auto ret = seen.insert(value.swapped()).second;
        fm_debug_assert(ret);
        return true;
    }
    return false;
}

void astar::push(const astar_edge& value, uint32_t dist)
{
    Q.emplace_back(value, dist);
    std::push_heap(Q.begin(), Q.end());
}

astar_edge_tuple astar::pop()
{
    fm_debug_assert(!Q.empty());
    auto ret = Q.back();
    std::pop_heap(Q.begin(), Q.end());
    return ret;
}

void astar::reserve(size_t count)
{
    Q.reserve(count);
    seen.reserve(2*count);
}

void astar::clear()
{
    Q.clear();
    seen.clear();
    mins.clear();
}

} // namespace floormat
