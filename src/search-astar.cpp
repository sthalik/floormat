#include "search-astar.hpp"
#include "search-octile.hpp"
#include "search-cache.hpp"
#include "grid-pool-registry.hpp"
#include "search-result.hpp"
#include "search.hpp"
#include "world.hpp"
#include "sweep-aabb.hpp"
#include "raycast-line.hpp"
#include "tile-defs.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "compat/array-size.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/format.hpp"
#include "compat/function2.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <bit>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>
#include <mg/Timeline.h>

namespace floormat {

struct astar::visited
{
    uint32_t dist = (uint32_t)-1;
    uint32_t prev = (uint32_t)-1;
    point pt;
};

struct astar::frontier
{
    uint32_t node = (uint32_t)-1;
    uint32_t f_score = (uint32_t)-1;
    uint32_t g_score = (uint32_t)-1;
};

// A lattice point the goal's seed search reached, `cost` px from `to` through the nodes head,
// where the search reached it, to tail.
struct astar::goal_seed
{
    uint32_t cost = (uint32_t)-1;
    uint32_t head = (uint32_t)-1, tail = (uint32_t)-1;
};

// One bucket per f modulo mask + 1, LIFO within a bucket. Pops in f order only while
// every live f lies at most mask above the lowest.
struct astar::bucket_queue
{
    // B[i].size() is the capacity. arrayRemoveSuffix() doesn't inline, so pop() would be a call.
    Array<frontier>* B;
    uint32_t* sizes;
    uint32_t mask;
    uint32_t cur = 0, hi = 0, count = 0;

    bool empty() const { return count == 0; }

    static CORRADE_NEVER_INLINE void grow(Array<frontier>& b)
    {
        arrayResize(b, NoInit, Math::max(b.size()*2, 64uz));
    }

    CORRADE_ALWAYS_INLINE void push(uint32_t id, uint32_t f, uint32_t g)
    {
        if (count == 0)
            cur = hi = f;
        else if (f < cur) // a seed below the earlier ones
            cur = f;
        else if (f > hi)
            hi = f;
        fm_debug3_assert(hi - cur <= mask);
        auto& b = B[f & mask];
        auto& n = sizes[f & mask];
        if (n == b.size()) [[unlikely]]
            grow(b);
        b.data()[n++] = frontier{ .node = id, .f_score = f, .g_score = g, };
        count++;
    }

    CORRADE_ALWAYS_INLINE frontier pop()
    {
        while (sizes[cur & mask] == 0)
            cur++;
        const auto i = cur & mask;
        const auto n = B[i].data()[--sizes[i]];
        count--;
        fm_debug3_assert(n.f_score == cur);
        return n;
    }
};

using visited = astar::visited;
using frontier = astar::frontier;
using bucket_queue = astar::bucket_queue;
using goal_seed = astar::goal_seed;
using endpoint = astar::endpoint;
using chunk_bitmaps = astar::chunk_bitmaps;

namespace {

void simplify_path(ArrayView<const point> src, Array<point>& dest)
{
    const auto size = (uint32_t)src.size();
    fm_assert(size > 0);

    arrayAppend(dest, src[0]);
    if (size < 2) [[unlikely]]
        return;

    auto last_vec = src[1] - src[0];

    for (auto i = 2u; i < size; i++)
    {
        const auto vec = src[i] - src[i-1];
        if (vec != last_vec)
        {
            if (dest.back() != src[i-1])
                arrayAppend(dest, src[i-1]);
            last_vec = vec;
        }
    }

    if (dest.back() != src.back())
        arrayAppend(dest, src.back());
}

struct segment_steps { Vector2i step; uint32_t count; };

// A run of steps of s px. The seed and the edge to the goal are a step of their own.
segment_steps steps_of_segment(point a, point b, int s)
{
    const auto d = b - a;
    const auto ad = Math::abs(d);
    const auto len = Math::max(ad.x(), ad.y());
    if (len == 0)
        return { {}, 0 };
    if (d.x() % s == 0 && d.y() % s == 0 && (ad.x() == 0 || ad.y() == 0 || ad.x() == ad.y()))
        return { d / (len / s), (uint32_t)(len / s) };
    return { d, 1 };
}

int8_t calc_turn(point a, point b, point c)
{
    const auto u = b - a, v = c - b;
    const auto x = (int64_t)u.x() * v.y() - (int64_t)u.y() * v.x();
    return (int8_t)((x > 0) - (x < 0));
}

bool same_ray(Vector2i a, Vector2i b)
{
    return (int64_t)a.x() * b.y() == (int64_t)a.y() * b.x()
           && (int64_t)a.x() * b.x() + (int64_t)a.y() * b.y() > 0;
}

// Han's order: the smaller angle from ref, and on the same ray, the farther one.
struct corner_pick
{
    Vector2i vec;
    double angle = 0;
    bool found = false;

    static double angle_of(Vector2i ref, Vector2i v)
    {
        const auto cross = (double)((int64_t)ref.x() * v.y() - (int64_t)ref.y() * v.x());
        const auto dot = (double)((int64_t)ref.x() * v.x() + (int64_t)ref.y() * v.y());
        return std::atan2(std::abs(cross), dot);
    }

    bool improved_by(Vector2i v, double a) const
    {
        if (!found)
            return true;
        if (same_ray(v, vec))
            return (int64_t)v.x() * v.x() + (int64_t)v.y() * v.y() > (int64_t)vec.x() * vec.x() + (int64_t)vec.y() * vec.y();
        return a < angle;
    }
};

struct dir_step { Vector2i vec, dir; uint32_t len, window_mask; };

constexpr uint32_t window_mask(int dx, int dy)
{
    const auto bit = [](int x, int y) { return 1u << ((y + 1)*3 + x + 1); };
    return bit(dx, dy) | (dx && dy ? bit(dx, 0) | bit(0, dy) : 0u);
}

// Diagonal and cardinal alternate: the main loop takes them in pairs.
constexpr Vector2i unit_directions[8] = {
    { -1, -1 }, {  0, -1 }, {  1, -1 }, { -1,  0 },
    {  1,  1 }, {  1,  0 }, { -1,  1 }, {  0,  1 },
};

static_assert([] {
    for (auto i = 0uz; i < 8; i++)
        for (auto j = 0uz; j < i; j++)
            if (unit_directions[i] == unit_directions[j])
                return false;
    return true;
}());

std::array<dir_step, 8> make_directions(const Search::octile& oct)
{
    std::array<dir_step, 8> array;
    for (auto i = 0uz; i < 8; i++)
    {
        const auto d = unit_directions[i];
        array[i] = { d * (int)oct.axial, d, d.x() && d.y() ? oct.diag : oct.axial, window_mask(d.x(), d.y()) };
    }
    return array;
}

void set_result_from_idx(path_search_result& result,
                         Array<point>& temp_nodes, const Array<visited>& nodes,
                         point from, const uint32_t idx)
{
    fm_assert(idx != (uint32_t)-1);

    uint32_t len = 0;
    for (auto i = idx; i != (uint32_t)-1; i = nodes[i].prev)
        len++;

    const auto& to_node = nodes[idx];
    result.set_cost(to_node.dist);

    arrayClear(temp_nodes);
    arrayReserve(temp_nodes, len + 2);
    arrayClear(result.raw_path());
    arrayReserve(result.raw_path(), len + 2);

    for (auto i = idx; i != (uint32_t)-1; i = nodes[i].prev)
        arrayAppend(temp_nodes, nodes[i].pt);
    arrayAppend(temp_nodes, from);

    std::reverse(temp_nodes.begin(), temp_nodes.end());
    simplify_path(temp_nodes, result.raw_path());
    arrayClear(temp_nodes);
}

// A pop's 3x3 window. When all 8 neighbours lie in the pop's chunk, chunk_idx is that chunk's
// index and tile_idx the pop's cell in the cache, else chunk_idx is (uint32_t)-1.
struct neighbourhood
{
    uint32_t window;
    uint32_t chunk_idx, tile_idx;
    Vector2i pix;
};

} // namespace

// One bitmap pointer per chunk of the cache rectangle. Nothing marks a grid stale
// during a search, so a pointer stays valid until it ends. Null: not resolved, or no chunk.
struct astar::chunk_bitmaps
{
    world& w;
    Grid::Pass::Pool& pool;
    Search::cache& cache;
    const astar::pred& p;
    const uint8_t** bits;
    uint32_t div_size, div_shift, div_count;

    CORRADE_ALWAYS_INLINE bool test_bit(const uint8_t* b, Vector2ui pix) const
    {
        fm_debug3_assert(pix.x() < chunk_size_xy && pix.y() < chunk_size_xy);
        const auto pos = pix >> div_shift;
        const auto i = pos.y() * div_count + pos.x();
        return b[i >> 3] & (1u << (i & 7));
    }

    CORRADE_ALWAYS_INLINE bool test_bit(const uint8_t* b, point pt) const
    {
        return test_bit(b, Vector2ui(Vector2i(intra_coord{pt})));
    }

    CORRADE_NEVER_INLINE const uint8_t* resolve(point pt, size_t chunk_idx)
    {
        auto* c = w.at(pt.chunk3());
        if (!c)
            return nullptr;
        auto grid = pool[*c];
        grid.build_if_stale();
        return bits[chunk_idx] = grid.bits().data;
    }

    CORRADE_NEVER_INLINE bool is_passable_slow(point pt, size_t chunk_idx)
    {
        if (const auto* b = resolve(pt, chunk_idx))
            return test_bit(b, pt);
        return cache.is_passable_for_bbox(w, pool, pt, p);
    }

    // Next to the chunk edge, the neighbours lie in up to three other chunks. 0 while one of
    // their bitmaps is unresolved: resolving it here could build a grid that no move reads.
    CORRADE_NEVER_INLINE uint32_t window_edge(point pt, Vector2i pix)
    {
        constexpr int cs = (int)chunk_size_xy;
        const auto step = (int)div_size;
        const auto ch = Vector2i(pt.chunk());
        Vector2i chunk[3];
        Vector2ui pos[3];
        for (int k = 0; k < 3; k++)
        {
            const auto q = pix + Vector2i{(k - 1)*step};
            const auto off = Vector2i{floor_div(q.x(), cs), floor_div(q.y(), cs)};
            chunk[k] = ch + off;
            pos[k] = Vector2ui(q - off*cs);
        }
        uint32_t win = 0;
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
            {
                const auto* b = bits[cache.get_chunk_index({chunk[c].x(), chunk[r].y()})];
                if (!b)
                    return 0;
                win |= (uint32_t)test_bit(b, Vector2ui{pos[c].x(), pos[r].y()}) << (r*3 + c);
            }
        return win;
    }

    // Bit (dy+1)*3 + dx+1 is the cell holding pt + (dx, dy)*div_size.
    CORRADE_ALWAYS_INLINE neighbourhood window(point pt)
    {
        constexpr int cs = (int)chunk_size_xy;
        const auto step = (int)div_size;
        const auto pix = Vector2i(intra_coord{pt});
        if ((uint32_t)(pix.x() - step) >= (uint32_t)(cs - 2*step) || (uint32_t)(pix.y() - step) >= (uint32_t)(cs - 2*step)) [[unlikely]]
            return { window_edge(pt, pix), (uint32_t)-1, 0, pix };
        const auto chunk_idx = cache.get_chunk_index(Vector2i(pt.chunk()));
        const auto tile_idx = cache.get_tile_index(intra_coord{pt});
        const auto* b = bits[chunk_idx];
        if (!b) [[unlikely]]
            if (!(b = resolve(pt, chunk_idx)))
                return { 0, chunk_idx, tile_idx, pix };
        uint32_t win = 0;
        // A row is one uint64_t. A narrower one would read past the last row.
        if (constexpr uint32_t row_cells = 64; div_count == row_cells) [[likely]]
        {
            const auto cell = Vector2ui(pix) / (uint32_t)(cs/row_cells);
            fm_debug3_assert(cell.x() - 1 < div_count - 2 && cell.y() - 1 < div_count - 2);
            for (uint32_t r = 0; r < 3; r++)
            {
                uint64_t row;
                std::memcpy(&row, b + (cell.y() - 1 + r)*(row_cells/8), sizeof row);
                if constexpr (std::endian::native == std::endian::big)
                    row = std::byteswap(row);
                win |= (uint32_t)((row >> (cell.x() - 1)) & 7) << r*3;
            }
        }
        else
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++)
                    win |= (uint32_t)test_bit(b, Vector2ui(pix + Vector2i{c - 1, r - 1}*step)) << (r*3 + c);
        return { win, chunk_idx, tile_idx, pix };
    }

    CORRADE_ALWAYS_INLINE bool is_passable(point pt, size_t chunk_idx)
    {
        if (const auto* b = bits[chunk_idx]) [[likely]]
            return test_bit(b, pt);
        return is_passable_slow(pt, chunk_idx);
    }

    CORRADE_ALWAYS_INLINE bool is_passable(point pt)
    {
        return is_passable(pt, cache.get_chunk_index(Vector2i(pt.chunk())));
    }

    CORRADE_ALWAYS_INLINE bool is_passable_diag(point a, point b, Vector2i vec, size_t b_chunk_idx)
    {
        return is_passable(b, b_chunk_idx)
            && is_passable(a + Vector2i{vec.x(), 0})
            && is_passable(a + Vector2i{0, vec.y()});
    }
};

struct astar::endpoint
{
    point pt;
    Vector2 center, own_lo, own_hi;
    chunk* c;
    std::array<chunk*, 8> neighbors;

    endpoint(world& w, point pt, Vector2ui own_size) :
        pt{pt}, center{intra_coord{pt}.center_shifted()},
        own_lo{own_size/2u}, own_hi{Vector2(own_size) - own_lo},
        c{w.at(pt.chunk3())}, neighbors{w.neighbors(pt.chunk3())}
    {
    }

    bool is_passable(const pred& p) const
    {
        return Search::is_passable_(c, neighbors, center - own_lo, center + own_hi, p);
    }

    // exact, so a goal whose cell bit is clear stays reachable
    bool is_passable_to(point other, const pred& p) const
    {
        const auto other_center = center + Vector2(other - pt);
        return Search::is_passable_(c, neighbors,
                                    Math::min(center, other_center) - own_lo,
                                    Math::max(center, other_center) + own_hi, p);
    }
};

struct astar::closest
{
    visited to_node;
    uint64_t closest_dist_sq = (uint64_t)-1;
    uint32_t closest_idx = (uint32_t)-1;
};

namespace {

// Not arrayResize(): a growable array's header puts every fourth node across a cache line.
CORRADE_NEVER_INLINE void grow_nodes(Array<visited>& nodes, uint32_t count)
{
    auto a = Array<visited>{NoInit, Math::max(nodes.size()*2, 64uz)};
    std::copy_n(nodes.data(), count, a.data());
    nodes = move(a);
}

// nodes.size() is the capacity and `count` the size. arrayAppend() would call arrayGrowBy()
// out of line for every node.
CORRADE_ALWAYS_INLINE uint32_t append_node(Array<visited>& nodes, uint32_t& count, const visited& node)
{
    if (count == nodes.size()) [[unlikely]]
        grow_nodes(nodes, count);
    nodes.data()[count] = node;
    return count++;
}

void print_push(uint32_t dist, point pt)
{
    DBG_nospace << " path:" << dist
                << " pos:" << Vector3i(pt.coord())
                << ";" << pt.offset();
}

template<bool IsDiagonal, int Debug>
CORRADE_ALWAYS_INLINE
void do_dir(chunk_bitmaps& bm, Search::cache& cache,
            Array<visited>& nodes, uint32_t& node_count, bucket_queue& Q,
            dir_step dirs, Search::octile oct,
            Vector2i goal_delta, point cur_pt,
            uint32_t cur_idx, uint32_t cur_dist,
            uint32_t max_dist, const neighbourhood& cells)
{
    const auto [vec, dir, len, mask] = dirs;
    const auto window = cells.window;

    if (window && (window & mask) != mask)
        return;

    const auto dist = cur_dist + len;
    // Equals new_pt - to: both branches below put new_pt at cur_pt's pixel + vec.
    uint32_t f_score = dist + oct(goal_delta + vec);
    if (f_score >= max_dist) [[unlikely]]
        return;

    const bool same_chunk = cells.chunk_idx != (uint32_t)-1;
    const auto new_pt = same_chunk ? point{cur_pt.chunk3(), intra_coord{cells.pix + vec}}
                                   : point::normalize_coords(cur_pt, vec);

    uint32_t chunk_idx, tile_idx;
    if (same_chunk) [[likely]]
    {
        // The cache's cells are one step wide.
        chunk_idx = cells.chunk_idx;
        tile_idx = (uint32_t)((ptrdiff_t)cells.tile_idx + (ptrdiff_t)dir.y() * cache.div_count_ + dir.x());
    }
    else
    {
        chunk_idx = cache.get_chunk_index(Vector2i(new_pt.chunk()));
        tile_idx = cache.get_tile_index(intra_coord{new_pt});
    }
    auto new_idx = cache.lookup_index(chunk_idx, tile_idx);

    if (new_idx != (uint32_t)-1)
        if (nodes[new_idx].dist <= dist)
            return;

    // A window of 0 can also mean nine blocked cells, which the per-cell tests find too.
    if (!window)
    {
        bool passable;
        if (IsDiagonal)
            passable = bm.is_passable_diag(cur_pt, new_pt, vec, chunk_idx);
        else
            passable = bm.is_passable(new_pt, chunk_idx);
        if (!passable)
            return;
    }

    if (new_idx == (uint32_t)-1)
    {
        new_idx = append_node(nodes, node_count, visited{
            .dist = dist,
            .prev = cur_idx,
            .pt = new_pt,
        });
        cache.add_index(chunk_idx, tile_idx, new_idx);
    }
    else
    {
        auto& n = nodes[new_idx];
        n.dist = dist;
        n.prev = cur_idx;
    }

    if constexpr (Debug >= 3)
        print_push(dist, new_pt);

    Q.push(new_idx, f_score, dist);
};

void print_closest(point cur, point to, uint32_t dist)
{
    DBG_nospace << "closest node"
                << " px:" << point::distance(cur, to) << " path:" << dist
                << " pos:" << cur;
}

void print_result(const path_search_result& result, point from, point to)
{
    auto d0_ =
        Vector2i(Math::abs(from.coord() - to.coord())) * iTILE_SIZE2
        + Vector2i(Math::abs(Vector2i(from.offset()) - Vector2i(to.offset())));
    auto d0 = (uint32_t)d0_.length();
    char buf[128];
    size_t len = 0;
    const auto time = result.time() * 1e3f;
    const auto d = result.cost();
    if (result.is_found())
        len = snformat(buf, "Dijkstra: found in {:.2f} ms "
                            "len:{} len0:{} ratio:{:.4}\n",
                       time, d, d0,
                       d > 0 && d0 > 0 ? (float)d/(float)d0 : 1);
    else if (!result.path().isEmpty())
    {
        // dist == 0 is legitimate: the {0,0} seed costs nothing when the
        // start lies on the node lattice, and it can remain the closest node
        fm_assert(d != (uint32_t)-1);
        len = snformat(buf, "Dijkstra: no path found in {:.2f} ms "
                            "closest:{} len:{} len0:{} ratio:{:.4}\n",
                       time, result.distance(), d, d0,
                       d0 > 0 ? (float)d/(float)d0 : 1);
    }
    if (len)
    {
        len = Math::min(len, array_size(buf)-1);
        std::fwrite(buf, len, 1, stdout);
        std::fflush(stdout);
    }
}

constexpr inline int seed_search_radius = 2*tile_size_xy;

// A goal touching a collider on its high side has a free node up to two cells less 2 px
// away on that axis, as a cell's bit needs every position in the cell free.
constexpr int goal_radius(int step) { return 2*step - 2; }
constexpr uint64_t goal_thres_sq(int step) { return 2 * (uint64_t)goal_radius(step) * (uint64_t)goal_radius(step); }
constexpr int goal_seed_radius(int step) { return seed_search_radius + step - 1; }

bool can_reach_goal(chunk_bitmaps& bm, const endpoint& start, const endpoint& goal)
{
    const auto step = (int)bm.div_size;
    const auto to = goal.pt;
    // Nodes lie on from's tile centre plus multiples of step. Over 64 px, that isn't to's.
    const auto origin = point{start.pt.coord(), {}};
    const auto off = to - origin;
    const auto lo = Vector2i{floor_div(off.x(), step), floor_div(off.y(), step)} * step;
    const auto n = goal_radius(step) / step + 2;
    for (int j = -n; j <= n; j++)
        for (int i = -n; i <= n; i++)
        {
            const auto pt = point::normalize_coords(origin, lo + Vector2i{i, j} * step);
            if (point::distance_squared(pt, to) <= goal_thres_sq(step)
                && bm.cache.is_passable_for_bbox(bm.w, bm.pool, pt, bm.p)
                && goal.is_passable_to(pt, bm.p))
                return true;
        }
    return false;
}

} // namespace

// For an end in a corridor narrower than the bbox plus a cell. Every cell along it holds a
// blocked position, so its bit is clear wherever the lattice lies. Dijkstra over integer
// positions up to seed_search_radius per axis from `source`, against the colliders themselves,
// stops at every set cell it pops. cf. HPA*'s start insertion (Botea et al. 2004, "Near Optimal
// Hierarchical Path-Finding", 3.2.1).
struct astar::seed_search
{
    static constexpr int R = seed_search_radius, W = 2*R + 1;
    static constexpr uint32_t free = 1, reached = 2, dir_shift = 2, g_shift = 5;

    astar& a;
    chunk_bitmaps& bm;
    bucket_queue& Q;
    endpoint source;
    // from's tile centre. From to's, the lattice is off by half a tile at 128 px and over.
    point origin;
    Search::octile oct{bm.div_size};
    int step = (int)bm.div_size;
    Vector2i phase{};

    static uint32_t index(Vector2i d) { return (uint32_t)((d.y() + R)*W + d.x() + R); }
    static Vector2i delta(uint32_t i) { return Vector2i{(int)(i % W), (int)(i / W)} - Vector2i{R}; }

    bool is_free(Vector2i d) const { return a.seed_search_state.data()[index(d)] & free; }

    // Rounding pt down to the lattice leaves its cell at steps over 32 px.
    point lattice_point(point pt) const
    {
        const auto pix = Vector2i(intra_coord{pt});
        const auto s = point{pt.chunk3(), intra_coord{pix - pix % step + phase}};
        fm_debug_assert(((s - origin) % step).isZero());
        return s;
    }

    uint32_t node_at(point s) const
    {
        auto& cache = bm.cache;
        return cache.lookup_index(cache.get_chunk_index(Vector2i(s.chunk())), cache.get_tile_index(intra_coord{s}));
    }

    // Rounded once: (g >> shift) + oct(pt, s) can be 1 below oct(s, to), which a goal seed's
    // cost must not be.
    uint32_t seed_dist(uint32_t g, point pt, point s) const
    {
        const auto d = Vector2ui{Math::abs(s - pt)};
        const auto mn = Math::min(d.x(), d.y()), mx = Math::max(d.x(), d.y());
        return (g + oct.axial*mx + (oct.diag - oct.axial)*mn) >> oct.shift;
    }

    uint32_t g_at(point pt) const
    {
        const auto d = pt - source.pt;
        if (Math::abs(d.x()) > R || Math::abs(d.y()) > R)
            return (uint32_t)-1;
        const auto s = a.seed_search_state.data()[index(d)];
        return s & reached ? s >> g_shift : (uint32_t)-1;
    }

    // pt and every turn on the way back to `source`, which is left out.
    template<typename F>
    void for_each_turn(point pt, F&& fn) const
    {
        const auto* const state = a.seed_search_state.data();
        auto d = pt - source.pt;
        auto dir = (uint32_t)-1;
        while (!d.isZero())
        {
            const auto s = state[index(d)];
            fm_debug_assert(s & reached);
            const auto k = (s >> dir_shift) & 7;
            if (k != dir)
            {
                fn(source.pt + d, s >> g_shift);
                dir = k;
            }
            d -= unit_directions[k];
        }
    }

    // Returns the node at pt, linked back to `source`, -1 at `source`. These nodes are never in
    // the cache or the queue.
    uint32_t add_path(point pt)
    {
        auto head = (uint32_t)-1, tail = head;
        for_each_turn(pt, [&](point q, uint32_t g) {
            const auto n = append_node(a.nodes, a.node_count, { .dist = g >> oct.shift, .prev = (uint32_t)-1, .pt = q, });
            if (tail == (uint32_t)-1)
                head = n;
            else
                a.nodes[tail].prev = n;
            tail = n;
        });
        return head;
    }

    // The same nodes linked the other way, tail reached through head.
    goal_seed add_goal_path(point pt, uint32_t cost)
    {
        auto gs = goal_seed{ .cost = cost, };
        for_each_turn(pt, [&](point q, uint32_t) {
            const auto n = append_node(a.nodes, a.node_count, { .prev = gs.tail, .pt = q, });
            if (gs.head == (uint32_t)-1)
                gs.head = n;
            gs.tail = n;
        });
        return gs;
    }

    template<typename F>
    CORRADE_ALWAYS_INLINE void flood(F&& at_set_cell)
    {
        auto& rects = a.seed_rects;
        const auto ch = source.pt.chunk3();
        arrayClear(rects);
        arrayReserve(rects, 16);
        Search::is_passable_(source.c, source.neighbors,
                             source.center - Vector2{(float)R} - source.own_lo,
                             source.center + Vector2{(float)R} + source.own_hi,
                             [&](chunk& c, collision_data data, Range2D r) {
            if (bm.p(c, data, r) == path_search_continue::pass)
                return path_search_continue::pass;
            // r is in c's frame
            const auto sc = c.coord();
            const auto off = Vector2{(float)(sc.x - ch.x), (float)(sc.y - ch.y)} * chunk_size<float>;
            arrayAppend(rects, Range2D{r.min() + off, r.max() + off});
            return path_search_continue::pass;
        });

        auto& state = a.seed_search_state;
        constexpr auto size = (uint32_t)W*W;
        fm_assert((uint64_t)size * oct.diag < 1u << (32 - g_shift));
        if (state.size() < size)
            state = Array<uint32_t>{NoInit, size};
        std::fill_n(state.data(), size, free);
        // rect_intersects is strict: the box at d overlaps r when
        // r.min - own_hi < source.center + d < r.max + own_lo on both axes
        for (const auto& r : rects)
        {
            const auto lo = Math::max(Vector2i(Math::floor(r.min() - source.own_hi - source.center)) + Vector2i{1}, Vector2i{-R});
            const auto hi = Math::min(Vector2i(Math::ceil(r.max() + source.own_lo - source.center)) - Vector2i{1}, Vector2i{R});
            for (auto y = lo.y(); y <= hi.y(); y++)
            {
                auto* const row = state.data() + index({0, y});
                for (auto x = lo.x(); x <= hi.x(); x++)
                    row[x] &= ~free;
            }
        }

        phase = Vector2i(intra_coord{origin}) % step;
        // the source's own box was checked against the same colliders before seeding
        fm_debug_assert(is_free({}));
        state.data()[index({})] = free | reached;
        fm_debug_assert(Q.empty());
        Q.push(index({}), 0, 0);

        while (!Q.empty())
        {
            const auto cur = Q.pop();
            const auto g = cur.g_score;
            if (g > state.data()[cur.node] >> g_shift)
                continue;
            const auto d = delta(cur.node);
            const auto pt = source.pt + d;
            if (bm.is_passable(pt))
            {
                at_set_cell(pt, g);
                continue;
            }
            for (auto k = 0u; k < 8; k++)
            {
                const auto u = unit_directions[k];
                const auto e = d + u;
                if (Math::abs(e.x()) > R || Math::abs(e.y()) > R)
                    continue;
                const auto diagonal = u.x() && u.y();
                const auto g2 = g + (diagonal ? oct.diag : oct.axial);
                auto& s = state.data()[index(e)];
                if ((s & reached) && (s >> g_shift) <= g2)
                    continue;
                if (!is_free(e) || (diagonal && (!is_free(d + Vector2i{u.x(), 0}) || !is_free(d + Vector2i{0, u.y()}))))
                    continue;
                s = free | reached | (k << dir_shift) | (g2 << g_shift);
                Q.push(index(e), g2, g2);
            }
        }
    }

    // Leaves the seeds it added or lowered in temp_nodes, as lattice points, and returns how many.
    CORRADE_NEVER_INLINE uint32_t run()
    {
        auto& nodes = a.nodes;
        auto& cache = bm.cache;
        auto& seed_from = a.temp_nodes;
        arrayClear(seed_from);
        flood([&](point pt, uint32_t g) {
            const auto s = lattice_point(pt);
            fm_debug_assert(bm.is_passable(s));
            const auto dist = seed_dist(g, pt, s);
            const auto chunk_idx = cache.get_chunk_index(Vector2i(s.chunk()));
            const auto tile_idx = cache.get_tile_index(intra_coord{s});
            const auto idx = cache.lookup_index(chunk_idx, tile_idx);
            if (idx == (uint32_t)-1)
                cache.add_index(chunk_idx, tile_idx, append_node(nodes, a.node_count, { .dist = dist, .prev = (uint32_t)-1, .pt = s, }));
            else if (dist < nodes[idx].dist)
                nodes[idx].dist = dist;
            else
                return;
            arrayAppend(seed_from, pt);
        });

        // A seed lowered twice was appended twice. Its dist is the later position's.
        uint32_t count = 0;
        for (auto i = 0u; i < seed_from.size(); i++)
        {
            const auto pt = seed_from[i];
            const auto s = lattice_point(pt);
            const auto idx = node_at(s);
            if (nodes[idx].dist != seed_dist(g_at(pt), pt, s))
                continue;
            const auto head = add_path(pt);
            nodes[idx].prev = head;
            seed_from[count++] = s;
        }
        return count;
    }

    // seed_index has a slot for each lattice point s up to R + step - 1 px per axis from
    // `source`, n per side, at (s - source + off) >> shift.
    CORRADE_NEVER_INLINE void run_goal()
    {
        auto& seeds = a.goal_seeds;
        auto& seed_from = a.temp_nodes;
        auto* const seed_index = a.goal_seed_index.data();
        const auto n = a.goal_seed_n;
        const auto off = a.goal_seed_off;
        std::fill_n(seed_index, n*n, (uint32_t)-1);
        arrayClear(seeds);
        arrayReserve(seeds, 16);
        arrayClear(seed_from);
        flood([&](point pt, uint32_t g) {
            const auto s = lattice_point(pt);
            fm_debug_assert(bm.is_passable(s));
            const auto cost = seed_dist(g, pt, s);
            const auto rel = Vector2ui(s - source.pt + off);
            fm_debug_assert(rel.x() < n*(uint32_t)step && rel.y() < n*(uint32_t)step);
            auto& slot = seed_index[(rel.y() >> oct.shift)*n + (rel.x() >> oct.shift)];
            if (slot == (uint32_t)-1)
            {
                slot = (uint32_t)seeds.size();
                arrayAppend(seeds, goal_seed{ .cost = cost, });
                arrayAppend(seed_from, pt);
            }
            else if (cost < seeds[slot].cost)
            {
                seeds[slot].cost = cost;
                seed_from[slot] = pt;
            }
        });
        for (auto i = 0u; i < seeds.size(); i++)
            seeds[i] = add_goal_path(seed_from[i], seeds[i].cost);
        arrayClear(seed_from);
    }
};

astar::astar() :
    _cache{InPlaceInit},
    _pool_registry{InPlaceInit, (uint32_t)tile_size_xy/4}
{
}

astar::~astar() noexcept = default;

void astar::reserve(size_t capacity)
{
    if (nodes.size() < capacity)
        nodes = Array<visited>{NoInit, capacity};
}

Search::cache* astar::cache() { return &*_cache; }
Grid::PoolRegistry& astar::pool_registry() { return *_pool_registry; }

template<int Debug, bool FindClosestOnFailure>
path_search_result astar::Dijkstra(world& w, const point from, const point to,
                                   uint32_t max_dist, Vector2ui own_size,
                                   const pred& p)
{
    auto& pool = _pool_registry->pool_for(Math::max(own_size.x(), own_size.y()));
    return Dijkstra<Debug, FindClosestOnFailure>(w, pool, from, to, max_dist, own_size, p);
}

auto astar::make_queue(uint32_t bucket_count) -> bucket_queue
{
    if (buckets.size() < bucket_count)
        arrayResize(buckets, bucket_count);
    if (bucket_sizes.size() < bucket_count)
        bucket_sizes = Array<uint32_t>{ValueInit, bucket_count};
    else
        std::fill_n(bucket_sizes.data(), bucket_count, 0u);
    return bucket_queue{ .B = buckets.data(), .sizes = bucket_sizes.data(), .mask = bucket_count - 1, };
}

auto astar::make_bitmaps(world& w, Grid::Pass::Pool& pool, const pred& p) -> chunk_bitmaps
{
    auto& cache = *_cache;
    const auto params = pool.params();
    const auto nchunks = (size_t)cache.size.product();
    if (chunk_bits.size() < nchunks)
        chunk_bits = Array<const uint8_t*>{ValueInit, nchunks};
    else
        std::fill_n(chunk_bits.data(), nchunks, nullptr);

    return chunk_bitmaps{
        .w = w, .pool = pool, .cache = cache, .p = p,
        .bits = chunk_bits.data(),
        .div_size = params.div_size(),
        .div_shift = (uint32_t)std::countr_zero(params.div_size()), // xxx todo assumes power of two chunk size
        .div_count = params.div_count,
    };
}

bool astar::seed_ring(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& start, point to, uint32_t max_dist)
{
    constexpr Vector2i seed_offsets[9] = {
        {  0,  0 },
        {  0, -1 },
        { -1,  0 },
        {  1,  0 },
        {  0,  1 },
        { -1, -1 },
        {  1, -1 },
        { -1,  1 },
        {  1,  1 },
    };

    auto& cache = bm.cache;
    const auto step = (int)bm.div_size;
    const auto oct = Search::octile{bm.div_size};
    const auto from = start.pt;
    const auto from_off = Vector2i(from.offset());
    const auto from_lo = Vector2i{floor_div(from_off.x(), step), floor_div(from_off.y(), step)} * step;

    bool seeded = false;
    for (auto off : seed_offsets)
    {
        auto pt = point::normalize_coords({from.coord(), {}}, from_lo + off*step);
        const auto chunk_idx = cache.get_chunk_index(Vector2i(pt.chunk()));
        const auto tile_idx = cache.get_tile_index(intra_coord{pt});
        auto dist = oct(from, pt);
        // the cell bit only covers the seed itself, so sweep the bbox over the
        // from -> seed motion, as at the goal
        if (cache.is_passable_for_bbox(bm.w, bm.pool, pt, bm.p) && start.is_passable_to(pt, bm.p))
        {
            auto idx = append_node(nodes, node_count, {.dist = dist, .prev = (uint32_t)-1, .pt = pt, });
            cache.add_index(chunk_idx, tile_idx, idx);
            seeded = true;
            uint32_t f_score = dist + oct(pt, to);
            if (dist < max_dist && f_score < max_dist)
                Q.push(idx, f_score, dist);
        }
    }
    return seeded;
}

visited astar::run_seed_search(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& start, point to, uint32_t max_dist)
{
    auto search = seed_search{ .a = *this, .bm = bm, .Q = Q, .source = start, .origin = point{start.pt.coord(), {}}, };
    const auto oct = search.oct;
    const auto count = search.run();
    auto link = visited{ .dist = max_dist, .pt = to, };
    if (const auto g = search.g_at(to); g != (uint32_t)-1 && (g >> oct.shift) < link.dist)
        link = { .dist = g >> oct.shift, .prev = search.add_path(to), .pt = to, };
    if (count > 0)
    {
        auto f_lo = (uint32_t)-1, f_hi = 0u;
        for (auto i = 0u; i < count; i++)
        {
            const auto& n = nodes[search.node_at(temp_nodes[i])];
            const auto f_score = n.dist + oct(n.pt, to);
            f_lo = Math::min(f_lo, f_score);
            f_hi = Math::max(f_hi, f_score);
        }
        // The seeds' f spreads by more than the 3x3's.
        if (const auto bucket_count = std::bit_ceil(Math::max(f_hi - f_lo, 2*oct.diag) + 2); bucket_count > Q.mask + 1)
            Q = make_queue(bucket_count);
        for (auto i = 0u; i < count; i++)
        {
            const auto idx = search.node_at(temp_nodes[i]);
            const auto dist = nodes[idx].dist, f_score = dist + oct(nodes[idx].pt, to);
            if (dist < max_dist && f_score < max_dist)
                Q.push(idx, f_score, dist);
        }
    }
    arrayClear(temp_nodes);
    return link;
}

// Returns the cheapest link to `to` it found: straight from `from`, or through a node already
// searched.
visited astar::run_goal_seed_search(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& start, const endpoint& goal)
{
    const auto step = (int)bm.div_size;
    const auto origin = point{start.pt.coord(), {}};
    const auto radius = goal_seed_radius(step);
    const auto to_rel = goal.pt - origin;
    goal_seed_n = (uint32_t)((2*radius + step - 1) / step + 1);
    goal_seed_off = to_rel - Vector2i{floor_div(to_rel.x() - radius, step),
                                      floor_div(to_rel.y() - radius, step)} * step;
    if (goal_seed_index.size() < goal_seed_n*goal_seed_n)
        goal_seed_index = Array<uint32_t>{NoInit, goal_seed_n*goal_seed_n};

    auto search = seed_search{ .a = *this, .bm = bm, .Q = Q, .source = goal, .origin = origin, };
    search.run_goal();
    auto link = visited{ .pt = goal.pt, };
    if (const auto g = search.g_at(start.pt); g != (uint32_t)-1)
        link = { .dist = g >> search.oct.shift, .prev = search.add_goal_path(start.pt, 0).tail, .pt = goal.pt, };
    // Nodes exist only once the main loop has emptied. Their dists are final then, except where
    // f reached max_dist, and cost >= h keeps those at max_dist or more.
    for (const auto& gs : goal_seeds)
    {
        fm_debug_assert(gs.head != (uint32_t)-1);
        const auto idx = search.node_at(search.lattice_point(nodes[gs.head].pt));
        if (idx == (uint32_t)-1)
            continue;
        nodes[gs.head].prev = idx;
        if (const auto d = nodes[idx].dist + gs.cost; d < link.dist)
            link = { .dist = d, .prev = gs.tail, .pt = goal.pt, };
    }
    return link;
}

template<int Debug, bool FindClosestOnFailure>
auto astar::expand_nodes(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& goal,
                         uint32_t max_dist, closest best) -> closest
{
    auto& cache = bm.cache;
    const auto step = (int)bm.div_size;
    const auto oct = Search::octile{bm.div_size};
    const auto directions = make_directions(oct);
    // copies: read through `this`, they would be reloaded after every call
    const auto seed_off = goal_seed_off;
    const auto seed_n = goal_seed_n;
    // A resume can add goal seeds. With none, a span of 0 fails every range check.
    const auto goal_seed_span = goal_seeds.isEmpty() ? 0u : seed_n * (uint32_t)step;
    const auto goal_seed_thres_sq = goal_seeds.isEmpty()
                                    ? goal_thres_sq(step)
                                    : Math::max(goal_thres_sq(step), 2 * (uint64_t)goal_seed_radius(step) * (uint64_t)goal_seed_radius(step));
    // a copy: read through `goal`, it would be reloaded after every call
    const auto to = goal.pt;
    // copies: `best` is passed by pointer, so a store to a node could change its fields
    auto to_node = best.to_node;
    auto closest_dist_sq = best.closest_dist_sq;
    auto closest_idx = best.closest_idx;

    while (!Q.empty())
    {
        const auto front = Q.pop();
        // Every later pop has f at least this one's, and f bounds any path through the node.
        if (front.f_score >= to_node.dist) [[unlikely]]
            break;
        const auto cur_idx = front.node;
        point cur_pt;
        uint32_t cur_dist;
        auto& n = nodes[cur_idx];
        if (front.g_score > n.dist)
            continue;
        cur_pt = n.pt;
        cur_dist = n.dist;

        const auto goal_delta = cur_pt - to;
        const auto goal_dist_sq = point::distance_squared(goal_delta);

        if constexpr (FindClosestOnFailure)
        {
            if (goal_dist_sq < closest_dist_sq)
            {
                closest_dist_sq = goal_dist_sq;
                closest_idx = cur_idx;

                if constexpr (Debug >= 2)
                    print_closest(cur_pt, to, cur_dist);
            }
        }

        if (goal_dist_sq <= goal_seed_thres_sq) [[unlikely]]
        {
            if (goal_dist_sq <= goal_thres_sq(step) && goal.is_passable_to(cur_pt, bm.p))
            {
                // Octile like the steps, so h never exceeds this edge's cost. h is 0 at the goal.
                const auto new_dist = cur_dist + oct(goal_delta);
                if (new_dist < to_node.dist)
                {
                    to_node.dist = new_dist;
                    to_node.prev = cur_idx;
                }
            }
            const auto rel = Vector2ui(goal_delta + seed_off);
            if (rel.x() < goal_seed_span && rel.y() < goal_seed_span)
            {
                const auto slot = goal_seed_index.data()[(rel.y() >> oct.shift)*seed_n + (rel.x() >> oct.shift)];
                if (slot != (uint32_t)-1)
                {
                    const auto& gs = goal_seeds.data()[slot];
                    if (const auto new_dist = cur_dist + gs.cost; new_dist < to_node.dist)
                    {
                        nodes[gs.head].prev = cur_idx;
                        to_node.dist = new_dist;
                        to_node.prev = gs.tail;
                    }
                }
            }
        }

        const auto cells = bm.window(cur_pt);

        for (auto i = 0u; i < 8; i += 2)
        {
            const auto d1 = directions.data()[i + 0],
                       d2 = directions.data()[i + 1];
            do_dir<1, Debug>(bm, cache, nodes, node_count, Q, d1, oct, goal_delta, cur_pt, cur_idx, cur_dist, max_dist, cells);
            do_dir<0, Debug>(bm, cache, nodes, node_count, Q, d2, oct, goal_delta, cur_pt, cur_idx, cur_dist, max_dist, cells);
        }
    }
    return { .to_node = to_node, .closest_dist_sq = closest_dist_sq, .closest_idx = closest_idx, };
}

template<int Debug>
path_search_result astar::make_result(const closest& best, point from, point to, uint32_t max_dist, float time)
{
    path_search_result result;
    result.set_div_size(_cache->div_size_);

    if (best.to_node.dist < max_dist)
    {
        const auto idx = append_node(nodes, node_count, best.to_node);
        result.set_found(true);
        result.set_distance(0);
        set_result_from_idx(result, temp_nodes, nodes, from, idx);
    }
    else if (best.closest_idx != (uint32_t)-1)
    {
        result.set_found(false);
        result.set_distance(point::distance(nodes[best.closest_idx].pt, to));
        set_result_from_idx(result, temp_nodes, nodes, from, best.closest_idx);
    }

    result.set_time(time);

    if constexpr (Debug >= 1)
        print_result(result, from, to);

    return result;
}

template<int Debug, bool FindClosestOnFailure>
path_search_result astar::Dijkstra(world& w, Grid::Pass::Pool& pool, const point from, const point to,
                                   uint32_t max_dist, Vector2ui own_size,
                                   const pred& p)
{
    reserve(initial_capacity);

    Timeline timeline;
    if constexpr(Debug > 0)
        timeline.start();

    node_count = 0;
    arrayClear(goal_seeds);

    if (from.coord().z() != to.coord().z()) [[unlikely]]
        return {};

    // todo try removing this eventually
    if (from.coord().z() != 0) [[unlikely]]
        return {};

    fm_debug3_assert(own_size < chunk_size<Vector2ui>);

    const auto params = pool.params();
    fm_assert(params.bbox_size >= Math::max(own_size.x(), own_size.y()));
    fm_assert(params.min_pass == pass_mode::pass);
    pool.maybe_mark_stale_all(w);

    // A step is one cell, so a cell holds one node.
    const auto step = (int)params.div_size();
    const auto oct = Search::octile{params.div_size()};
    auto& cache = *_cache;
    cache.allocate(from, max_dist, params.div_size());

    const auto start = endpoint{w, from, own_size}, goal = endpoint{w, to, own_size};
    if (!start.is_passable(p) || !goal.is_passable(p))
        return {};

    // Outside it, `to` lies over max_dist from `from`, and the cells around it can lie outside the
    // cache.
    const auto to_off = Vector2ui(Vector2i(to.chunk()) - cache.start - Vector2i{1});
    const auto to_in_cache = to_off.x() < cache.size.x() - 2 && to_off.y() < cache.size.y() - 2;
    if constexpr (!FindClosestOnFailure)
        if (!to_in_cache)
            return {};

    auto bm = make_bitmaps(w, pool, p);
    // Seeds lie up to seed_radius px from `from` per axis, so their f spreads by up to twice its
    // octile. A step raises f by at most 2*diag.
    const auto seed_radius = 2*step - 1;
    auto Q = make_queue(std::bit_ceil(2*Math::max(oct(Vector2i{seed_radius}), oct.diag) + 2));

    const bool near_goal = to_in_cache && can_reach_goal(bm, start, goal);
    auto best = closest{ .to_node = { .dist = max_dist, .pt = to, }, };

    // A goal not near any node is reached only through the goal's seed search.
    if (to_in_cache && !near_goal)
    {
        if (const auto link = run_goal_seed_search(bm, Q, start, goal); link.dist < best.to_node.dist)
            best.to_node = link;
        if constexpr (!FindClosestOnFailure)
            if (goal_seeds.isEmpty() && best.to_node.dist == max_dist)
            {
                // Only the start's seed search can still reach `to`, as one of its positions. No node
                // is near the goal and there are no goal seeds, so A* can't lower that link.
                const auto d = Math::abs(to - from);
                if (Math::max(d.x(), d.y()) > seed_search_radius || bm.is_passable(from))
                    return {};
                if (const auto link = run_seed_search(bm, Q, start, to, max_dist); link.dist < best.to_node.dist)
                    best.to_node = link;
                if (best.to_node.dist == max_dist)
                    return {};
                return make_result<Debug>(best, from, to, max_dist, timeline.currentFrameTime());
            }
    }

    const bool ring_seeded = seed_ring(bm, Q, start, to, max_dist);
    if (!ring_seeded)
    {
        if (const auto link = run_seed_search(bm, Q, start, to, max_dist); link.dist < best.to_node.dist)
            best.to_node = link;
        if (Q.empty() && best.to_node.dist == max_dist)
            return {};
    }

    best = expand_nodes<Debug, FindClosestOnFailure>(bm, Q, goal, max_dist, best);

    // Where a corridor the lattice can't enter opens into a room, the 3x3 seeds, or the nodes
    // near the goal, can all lie in the room. From a set cell, a seed search would only seed that
    // cell.
    if (best.to_node.dist == max_dist)
    {
        if (near_goal && !bm.is_passable(to))
            if (const auto link = run_goal_seed_search(bm, Q, start, goal); link.dist < best.to_node.dist)
                best.to_node = link;
        if (best.to_node.dist == max_dist && ring_seeded && !bm.is_passable(from))
            if (const auto link = run_seed_search(bm, Q, start, to, max_dist); link.dist < best.to_node.dist)
                best.to_node = link;
        if (!Q.empty())
            best = expand_nodes<Debug, FindClosestOnFailure>(bm, Q, goal, max_dist, best);
    }

    return make_result<Debug>(best, from, to, max_dist, timeline.currentFrameTime());
}

template path_search_result astar::Dijkstra<0, true>(world&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<1, true>(world&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<2, true>(world&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<3, true>(world&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<0, true>(world&, Grid::Pass::Pool&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<1, true>(world&, Grid::Pass::Pool&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<2, true>(world&, Grid::Pass::Pool&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<3, true>(world&, Grid::Pass::Pool&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<0, false>(world&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<1, false>(world&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<2, false>(world&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<3, false>(world&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<0, false>(world&, Grid::Pass::Pool&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<1, false>(world&, Grid::Pass::Pool&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<2, false>(world&, Grid::Pass::Pool&, point, point, uint32_t, Vector2ui, const pred&);
template path_search_result astar::Dijkstra<3, false>(world&, Grid::Pass::Pool&, point, point, uint32_t, Vector2ui, const pred&);

auto astar::string_pull(world& w, path_search_result& result, Vector2ui own_size, const pred& p) -> string_pull_result
{
    return string_pull(w, _pool_registry->pool_for(Math::max(own_size.x(), own_size.y())), result, own_size, p);
}

auto astar::string_pull(world& w, Grid::Pass::Pool& pool, path_search_result& result,
                        Vector2ui own_size, const pred& p) -> string_pull_result
{
    uint32_t corners_added = 0, los_checks = 0;
    auto& sp = result.smoothings();
    arrayClear(sp);
    const auto& raw = result.raw_path();
    if (raw.isEmpty())
        return {};

    const auto own_lo = Vector2(own_size/2u), own_hi = Vector2(own_size) - own_lo;
    fm_assert(pool.params().bbox_size >= Math::max(own_size.x(), own_size.y()));
    fm_assert_equal(result.div_size(), pool.params().div_size());
    pool.maybe_mark_stale_all(w);
    const auto cell = (int)pool.params().div_size();

    auto& P = temp_nodes;
    arrayClear(P);
    arrayAppend(P, raw[0]);
    for (auto j = 0uz; j + 1 < raw.size(); j++)
    {
        const auto [step, count] = steps_of_segment(raw[j], raw[j+1], cell);
        for (auto t = 1u; t <= count; t++)
            arrayAppend(P, raw[j] + step * (int)t);
    }
    const auto n = (uint32_t)P.size();

    struct los_result { corner_pick best; bool blocked; };

    // The best corner of the rects blocking from -> to, grown by the bbox and rounded outward.
    // With limit, only corners at a smaller angle.
    const auto los = [&](point from, point to, Vector2i ref, const corner_pick* limit) {
        const auto ch = from.chunk3();
        const auto origin = point{ch, {}, {}};
        const auto o = from - origin;
        const auto line = rc::bresenham_line{o, to - origin};
        los_result res = {};
        los_checks++;
        find_swept_collider(w, ch, line, 0, line.steps(), own_size, p, &pool, [&](const swept_collider_result& r) {
            res.blocked = true;
            const auto x0 = (int32_t)Math::floor(r.rect.min().x() - own_hi.x()), x1 = (int32_t)Math::ceil(r.rect.max().x() + own_lo.x());
            const auto y0 = (int32_t)Math::floor(r.rect.min().y() - own_hi.y()), y1 = (int32_t)Math::ceil(r.rect.max().y() + own_lo.y());
            for (const auto c : { Vector2i{x0, y0}, Vector2i{x1, y0}, Vector2i{x0, y1}, Vector2i{x1, y1} })
            {
                const auto v = c - o;
                if (v.isZero())
                    continue;
                const auto a = corner_pick::angle_of(ref, v);
                if (limit && (a >= limit->angle || same_ray(v, limit->vec)))
                    continue;
                if (res.best.improved_by(v, a))
                    res.best = { .vec = v, .angle = a, .found = true };
            }
            return true;
        });
        return res;
    };

    const auto give_up = [&] {
        arrayClear(sp);
        arrayClear(P);
        return string_pull_result{ .corners_added = corners_added, .los_checks = los_checks, .smoothed = false };
    };

    arrayAppend(sp, path_search_result::waypoint{ P[0], 0 });
    int8_t turn = 0;
    // Algorithm 1 was written for grid cells. The bound keeps a cycle on rects from hanging.
    auto budget = 8*n + 64;
    uint32_t seg = 0, seg_last = n > 2 ? steps_of_segment(raw[0], raw[1], cell).count : 0;

    for (uint32_t i = 2; i < n; )
    {
        if (!budget--)
            return give_up();
        while (i > seg_last)
        {
            seg++;
            fm_debug_assert(seg + 1 < raw.size());
            seg_last += steps_of_segment(raw[seg], raw[seg+1], cell).count;
        }

        const auto O = sp.back().pt;
        const auto ref = P[i-1] - O;
        const auto res = los(O, P[i], ref, nullptr);
        if (res.blocked)
        {
            if (ref.isZero())
                return give_up();
            // A rect smaller than a step can lie between two lines Han takes as clear.
            auto pick = res.best;
            for (auto k = 0u; pick.found; k++)
            {
                const auto check = los(O, O + pick.vec, ref, &pick);
                if (!check.blocked)
                    break;
                if (k == 16)
                    return give_up();
                pick = check.best;
            }
            // Pixel rounding can clear the line to P[i-1] past a corner its exact segment clips. That
            // corner lies across ref from pick, so limit excludes it and no pick remains.
            if (!pick.found)
            {
                if (los(O, P[i-1], ref, nullptr).blocked)
                    return give_up();
                pick = { .vec = ref, .found = true };
            }
            const auto u = O + pick.vec;
            arrayAppend(sp, path_search_result::waypoint{ u, seg });
            corners_added++;
            turn = calc_turn(O, u, P[i]);
        }
        else if (sp.size() > 1 && calc_turn(sp[sp.size()-2].pt, O, P[i]) != turn)
        {
            arrayRemoveSuffix(sp, 1);
            if (sp.size() > 1)
                turn = calc_turn(sp[sp.size()-2].pt, sp.back().pt, P[i]);
        }
        else
            i++;
    }

    const auto last = (uint32_t)raw.size() - 1;
    if (sp.back().pt != P[n-1])
        arrayAppend(sp, path_search_result::waypoint{ P[n-1], last });
    else
        sp.back().raw_index = last;
    arrayClear(P);
    return { .corners_added = corners_added, .los_checks = los_checks, .smoothed = true };
}

} // namespace floormat
