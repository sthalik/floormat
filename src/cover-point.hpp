#pragma once
#include "point.hpp"
#include "search-pred.hpp"
#include "compat/defs.hpp"
#include "compat/safe-ptr.hpp"
#include <cr/ArrayView.h>

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
    ArrayView<const point> spots;
    uint32_t candidates = 0;
    uint32_t rejected = 0;
    bool found = false;
};

struct cover_point
{
    // Returns the first spot near q.pos where a body of q.body_size fits by pred and q.threat sees no
    // pixel of it. Candidates are the cells of the cover grid for params within q.radius of q.pos
    // where the body fits, the grid marks the cell hidden and the others under the body hidden or
    // blocked, and a blocker is near enough to push the body against. They are tried in order of
    // octile distance from q.pos. Each is pushed toward the threat until flush with its blocker, and when the threat sees
    // it there, it is moved sideways by up to two cells and pushed again. found, pos and axis
    // describe the first candidate accepted. candidates counts the candidates. rejected counts
    // those that reached a blocker and stayed seen, among the ones tried before pos, or among all of
    // them when found is false.
    // pred decides where the body fits, so a critter standing on a spot can rule it out. What hides
    // the body is decided without critters, as in the cover grids.
    // With list_spots, the search goes on past pos, and spots holds every accepted spot once, in the
    // order tried, starting with pos. Two candidates can be pushed to the same spot. The other fields
    // are the same as without list_spots. spots points into this cover_point, so it is valid until
    // the next call.
    cover_point_result find_cover_point(world& w, Grid::PoolRegistry& pools, Grid::Cover::Params params,
                                        const cover_query& q, const Search::pred& pred,
                                        bool list_spots = false);

    cover_point();
    ~cover_point() noexcept;
    fm_DISABLE_MOVE_COPY(cover_point);

private:
    struct Impl;
    safe_ptr<Impl> impl;
};

} // namespace floormat
