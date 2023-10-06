#pragma once
#include "global-coords.hpp"
#include <vector>

#include <tsl/robin_set.h>

namespace floormat {

struct astar_edge;

struct astar_hash {
    size_t operator()(const astar_edge& e) const;
};

struct astar_edge
{
    friend struct astar_equal;
    bool operator==(const astar_edge&) const noexcept;

    astar_edge(global_coords ch1, Vector2b off1, global_coords ch2, Vector2b off2);
    astar_edge(chunk_coords_ ch1, local_coords t1, Vector2b off1,
               chunk_coords_ ch2, local_coords t2, Vector2b off2);
    size_t hash() const;

private:
    int16_t from_cx, from_cy, to_cx, to_cy;
    int8_t from_cz, to_cz;
    uint8_t from_t, to_t;
    int8_t from_offx, from_offy, to_offx, to_offy;

    static constexpr auto INF = (uint32_t)-1;
};

struct astar_edge_tuple
{
    static constexpr auto MAX = (uint32_t)-1;
    friend bool operator<(const astar_edge_tuple& a, const astar_edge_tuple& b);

    astar_edge e;
    uint32_t dist = MAX;
};

struct astar final
{
    void reserve(size_t count);
    bool empty() const { return Q.empty(); }
    [[nodiscard]] bool add_seen(const astar_edge& value);
    void push(const astar_edge& value, uint32_t dist);
    astar_edge pop();
    void clear();

private:
    static constexpr bool StoreHash = true; // todo benchmark it
    tsl::robin_set<astar_edge,
                   astar_hash, std::equal_to<>,
                   std::allocator<astar_edge>,
                   StoreHash> seen;
    std::vector<astar_edge_tuple> Q;
};

} // namespace floormat
