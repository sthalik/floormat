#pragma once
#include "compat/defs.hpp"
#include "global-coords.hpp"
#include <vector>
#include <tsl/robin_set.h>
#include <tsl/robin_map.h>

namespace floormat {

struct astar_edge;

struct astar_hash {
    size_t operator()(const astar_edge& e) const;
};

struct astar_edge
{
    bool operator==(const astar_edge&) const noexcept;

    fm_DECLARE_DEFAULT_COPY_ASSIGNMENT_(astar_edge);
    astar_edge(global_coords coord1, Vector2b off1, global_coords coord2, Vector2b off2);
    astar_edge(chunk_coords_ ch1, local_coords t1, Vector2b off1,
               chunk_coords_ ch2, local_coords t2, Vector2b off2);
    size_t hash() const;
    astar_edge swapped() const;

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
    astar();
    [[nodiscard]] bool add_visited(const astar_edge& value);
    void push(const astar_edge& value, uint32_t dist);
    astar_edge_tuple pop();

    bool empty() const { return Q.empty(); }
    void reserve(size_t count);
    void clear();

private:
    struct edge_min { astar_edge prev; uint32_t len; };

    static constexpr bool StoreHash = true; // todo benchmark it
    tsl::robin_set<astar_edge,
                   astar_hash, std::equal_to<>,
                   std::allocator<astar_edge>,
                   StoreHash> seen;
    tsl::robin_map<astar_edge, edge_min, astar_hash> mins;
    std::vector<astar_edge_tuple> Q;
};

} // namespace floormat
