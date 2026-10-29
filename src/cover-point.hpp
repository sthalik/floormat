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
    /// the body's center, as Dijkstra takes it
    point pos;
    Vector2ui body_size;
    point threat;
    uint32_t radius;
};

struct cover_point_result
{
    /// The first accepted spot, when found.
    point pos;
    /// toward the threat; pos is flush against a blocker along it
    Vector2i axis;
    /// With list_spots, every accepted spot once, in the order tried, starting with pos. Points
    /// into the cover_point, so it is valid until the next find_cover_point() call.
    ArrayView<const point> spots;
    /// The number of candidates, including those never tried.
    uint32_t candidates = 0;
    /// The candidates pushed flush to a blocker and still seen, among those tried before pos, or
    /// among all of them when found is false.
    uint32_t rejected = 0;
    bool found = false;
};

struct cover_point
{
    /// @brief The first spot near q.pos where a body of q.body_size fits by pred, which counts
    /// critters, and q.threat sees no pixel of it.
    ///
    /// Candidates are the cover grid cells within q.radius that hide the body and have a blocker near
    /// enough to push it against, tried by octile distance from q.pos. Each is pushed toward the
    /// threat until flush, and if still seen, moved up to two cells sideways and pushed again.
    /// @param list_spots Go on past the first spot and list every accepted one in spots. The
    /// other fields stay as without it.
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
