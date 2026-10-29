#pragma once
#include "point.hpp"
#include "search-pred.hpp"
#include "compat/defs.hpp"
#include "compat/safe-ptr.hpp"

namespace floormat { class world; }
namespace floormat::Grid { class PoolRegistry; }
namespace floormat::Grid::Cover { struct Params; }

namespace floormat {

struct cover_query
{
    // the body's center, as Dijkstra takes it
    point pos;
    Vector2ui body_size;
    point threat;
    uint32_t radius;
};

struct cover_point_result
{
    point pos;
    // toward the threat; pos is flush against a blocker along it
    Vector2i axis;
    uint32_t candidates = 0;
    uint32_t rejected = 0;
    bool found = false;
};

struct cover_point
{
    // pred decides where the body fits, so a critter standing on a spot can rule it out. What hides
    // the body is decided without critters, as in the cover grids.
    cover_point_result find_cover_point(world& w, Grid::PoolRegistry& pools, Grid::Cover::Params params,
                                        const cover_query& q, const Search::pred& pred);

    cover_point();
    ~cover_point() noexcept;
    fm_DISABLE_MOVE_COPY(cover_point);

private:
    struct Impl;
    safe_ptr<Impl> impl;
};

} // namespace floormat
