#pragma once
#include "point.hpp"
#include "search-pred.hpp"
#include "compat/defs.hpp"
#include "compat/safe-ptr.hpp"
#include <cr/ArrayView.h>
#include <cr/Optional.h>
#include <mg/Range.h>

namespace floormat { class world; }
namespace floormat::Grid { class GridRegistry; }
namespace floormat::Grid::Cover { struct Params; }

namespace floormat {

struct cover_query
{
    /// the body's center, as Dijkstra takes it
    point pos;
    Vector2ui body_size;
    point threat;
    u32 radius;
    /// 0, or an odd number up to 9 of cover grid octants, centered on the bearing from a candidate to
    /// the threat, that must each hide the body, so a threat that moves a little still doesn't see it.
    /// An accepted spot must also stay hidden from the threat turned about it by up to octants/2.
    u8 octants = 0;
    /// 0, or how far in px an accepted spot may move along the face it is flush against, toward the
    /// side where the body is first seen. It stops at the last px where the body is still hidden,
    /// fits and is flush.
    u32 max_cover_end_dist = 0;
    /// When set, only cells whose center c has area->min() <= c - pos < area->max(), in px, are
    /// candidates. Areas that tile the plane together accept the spots one search without an area
    /// does, since each candidate is judged alone. Two areas can accept the same spot.
    Optional<Range2Di> area = {};
    /// The most octant fills the call may do, a stale grid's rebuild counting as one. A read that
    /// needs one more ends the call with out_of_fills before any candidate is tried.
    u32 max_fills = (u32)-1;
};

struct cover_spot
{
    point pos;
    /// toward the threat; pos is flush against a blocker along it
    Vector2i axis;
};

struct cover_point_result
{
    /// The first accepted spot, when found.
    point pos;
    /// toward the threat; pos is flush against a blocker along it
    Vector2i axis;
    /// With list_spots, every accepted spot once, in the order tried, starting with pos and axis.
    /// A spot two candidates reach keeps the first one's axis. Points into the cover_point, so it
    /// is valid until the next find_cover_point() call.
    ArrayView<const cover_spot> spots;
    /// The number of candidates, including those never tried.
    u32 candidates = 0;
    /// The candidates pushed flush to a blocker and still seen by the threat or a turned one,
    /// among those tried before pos, or among all of them when found is false.
    u32 rejected = 0;
    /// Octant fills the call did, rebuilds included.
    u32 fills = 0;
    bool found = false;
    /// The call stopped at q.max_fills and found nothing. The same query goes on from there, as the
    /// fills done stay in the grids.
    bool out_of_fills = false;
};

/// The cover grid octant whose bearing is nearest v's.
u32 nearest_octant(Vector2 v);

/// The unit vector along v's longer axis, pointing the way v does. x wins a tie, and a zero v gives {-1, 0}.
Vector2i axis_toward(Vector2i v);

/// The direction along the face of the blocker a cover spot with this axis is flush against.
Vector2i across_of(Vector2i axis);

/// Replaces out with the offsets, in tiles, of the tiles whose nearest point lies within radius px
/// of a tile's center, nearest first, then by y and x.
void nearest_tiles(u32 radius, Array<Vector2i>& out);

struct cover_point
{
    /// @brief The first spot near q.pos where a body of q.body_size fits by pred, which counts
    /// critters, and q.threat sees no pixel of it.
    ///
    /// Candidates are the cover grid cells within q.radius that hide the body and have a blocker near
    /// enough to push it against, tried by octile distance from q.pos.
    /// @param list_spots Go on past the first spot and list every accepted one in spots, leaving the
    /// other fields as without it.
    cover_point_result find_cover_point(world& w, Grid::GridRegistry& pools, Grid::Cover::Params params,
                                        const cover_query& q, const Search::pred& pred,
                                        bool list_spots = false);
    /// True when threat sees no pixel of a body of body_size centered on s.pos, tested as find_cover_point
    /// tests its spots.
    bool hidden_at(world& w, point threat, const cover_spot& s, Vector2ui body_size);
    /// q.pos, flush against a blocker along axis, moved along its face as find_cover_point moves its spots for q.
    /// The result is hidden from each point of also_from too, or is q.pos.
    point move_to_cover_end(world& w, const cover_query& q, Vector2i axis, const Search::pred& pred,
                            ArrayView<const point> also_from = {});

    cover_point();
    ~cover_point() noexcept;
    fm_DISABLE_MOVE_COPY(cover_point);

private:
    struct Impl;
    safe_ptr<Impl> impl;
};

} // namespace floormat
