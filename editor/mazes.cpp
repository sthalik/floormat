#include "mazes.hpp"
#include "src/world.hpp"
#include "src/tile-image.hpp"
#include "src/ground-atlas.hpp"
#include "src/grid-pass.hpp"
#include "src/search-octile.hpp"
#include "src/point.inl"
#include "src/intra-coord.inl"
#include "src/object.hpp"
#include "src/object-storage.inl"
#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/hole.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
#include "compat/floor-divmod.hpp"
#include <utility>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat::mazes {

namespace {

constexpr inline uint32_t maze_pitch = 2, room_min = 4, room_spread = 5;

enum : uint8_t { open_e = 1 << 0, open_s = 1 << 1 };
enum : uint8_t { wall_w = 1 << 0, wall_n = 1 << 1 };

struct rng
{
    uint64_t s;

    // splitmix64, then Lemire's multiply-shift onto [0, n)
    uint32_t operator()(uint32_t n)
    {
        s += 0x9e3779b97f4a7c15u;
        auto z = s;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
        z ^= z >> 31;
        return (uint32_t)(((z >> 32) * n) >> 32);
    }
};

struct cell_graph
{
    struct adjacent { uint32_t cell[4]; uint32_t count; };

    uint32_t w, h;
    Array<uint8_t> flags;

    cell_graph(uint32_t w, uint32_t h): w{w}, h{h}, flags{ValueInit, w*h} {}

    adjacent neighbors(uint32_t c) const
    {
        adjacent r{};
        const auto x = c % w, y = c / w;
        if (x > 0)
            r.cell[r.count++] = c - 1;
        if (x + 1 < w)
            r.cell[r.count++] = c + 1;
        if (y > 0)
            r.cell[r.count++] = c - w;
        if (y + 1 < h)
            r.cell[r.count++] = c + w;
        return r;
    }

    // one bit per wall, kept on the west or north cell of the pair
    bool is_open(uint32_t a, uint32_t b) const
    {
        const auto lo = Math::min(a, b), hi = Math::max(a, b);
        return (flags[lo] & (hi == lo + 1 ? open_e : open_s)) != 0;
    }

    void connect(uint32_t a, uint32_t b)
    {
        const auto lo = Math::min(a, b), hi = Math::max(a, b);
        flags[lo] |= hi == lo + 1 ? open_e : open_s;
    }

    uint32_t degree(uint32_t c) const
    {
        const auto nb = neighbors(c);
        uint32_t n = 0;
        for (auto i = 0u; i < nb.count; i++)
            n += is_open(c, nb.cell[i]);
        return n;
    }
};

void wilson(cell_graph& g, rng& r)
{
    const auto n = g.w*g.h;
    Array<uint8_t> in_tree{ValueInit, n};
    Array<uint32_t> next{NoInit, n};
    in_tree[0] = 1;
    for (uint32_t s = 0; s < n; s++)
    {
        // a revisit overwrites next[], which erases the loop
        for (auto c = s; !in_tree[c]; c = next[c])
        {
            const auto nb = g.neighbors(c);
            next[c] = nb.cell[r(nb.count)];
        }
        for (auto c = s; !in_tree[c]; c = next[c])
        {
            in_tree[c] = 1;
            g.connect(c, next[c]);
        }
    }
}

void prim(cell_graph& g, rng& r)
{
    enum : uint8_t { unseen, queued, seen };
    const auto n = g.w*g.h;
    Array<uint8_t> state{ValueInit, n};
    Array<uint32_t> frontier{NoInit, n};
    uint32_t top = 0;

    const auto visit = [&](uint32_t c) {
        state[c] = seen;
        const auto nb = g.neighbors(c);
        for (auto i = 0u; i < nb.count; i++)
            if (state[nb.cell[i]] == unseen)
            {
                state[nb.cell[i]] = queued;
                frontier[top++] = nb.cell[i];
            }
    };

    visit(0);
    while (top > 0)
    {
        const auto i = r(top);
        const auto c = frontier[i];
        frontier[i] = frontier[--top];
        const auto nb = g.neighbors(c);
        uint32_t cand[4], k = 0;
        for (auto j = 0u; j < nb.count; j++)
            if (state[nb.cell[j]] == seen)
                cand[k++] = nb.cell[j];
        g.connect(c, cand[r(k)]);
        visit(c);
    }
}

// The newest cell 3 times in 4. Always the newest (the recursive backtracker) has longer routes,
// but leaves long dead ends near the exit, which f >= C* keeps A* out of.
void growing_tree(cell_graph& g, rng& r)
{
    const auto n = g.w*g.h;
    Array<uint8_t> seen{ValueInit, n};
    Array<uint32_t> active{NoInit, n};
    uint32_t top = 0;
    const auto start = r(n);
    seen[start] = 1;
    active[top++] = start;
    while (top > 0)
    {
        const auto i = r(4) ? top - 1 : r(top);
        const auto c = active[i];
        const auto nb = g.neighbors(c);
        uint32_t cand[4], k = 0;
        for (auto j = 0u; j < nb.count; j++)
            if (!seen[nb.cell[j]])
                cand[k++] = nb.cell[j];
        if (k == 0)
        {
            active[i] = active[--top];
            continue;
        }
        const auto d = cand[r(k)];
        g.connect(c, d);
        seen[d] = 1;
        active[top++] = d;
    }
}

void braid(cell_graph& g, rng& r)
{
    const auto n = g.w*g.h;
    Array<uint32_t> order{NoInit, n};
    for (uint32_t i = 0; i < n; i++)
        order[i] = i;
    for (auto i = n - 1; i > 0; i--)
        std::swap(order[i], order[r(i + 1)]);

    for (auto c : order)
    {
        if (g.degree(c) != 1)
            continue;
        const auto nb = g.neighbors(c);
        uint32_t closed[4], dead[4], nc = 0, nd = 0;
        for (auto k = 0u; k < nb.count; k++)
            if (!g.is_open(c, nb.cell[k]))
            {
                closed[nc++] = nb.cell[k];
                if (g.degree(nb.cell[k]) == 1)
                    dead[nd++] = nb.cell[k];
            }
        g.connect(c, nd ? dead[r(nd)] : closed[r(nc)]);
    }
}

// Tiles [0, size]² so that the last row and column carry the east and south seal.
struct wall_map
{
    uint32_t size;
    Array<uint8_t> walls;

    explicit wall_map(uint32_t size): size{size}, walls{ValueInit, (size+1)*(size+1)} {}
    uint8_t& at(uint32_t x, uint32_t y) { return walls[y*(size+1) + x]; }
    uint8_t at(uint32_t x, uint32_t y) const { return walls[y*(size+1) + x]; }
};

wall_map draw_maze(const cell_graph& g)
{
    const auto size = g.w*maze_pitch;
    wall_map m{size};
    for (uint32_t y = 0; y <= size; y++)
        for (uint32_t x = 0; x <= size; x++)
        {
            const auto cx = x / maze_pitch, cy = y / maze_pitch;
            if (x % maze_pitch == 0 && y < size)
                if (x == 0 || x == size || !g.is_open(cy*g.w + cx - 1, cy*g.w + cx))
                    m.at(x, y) |= wall_w;
            if (y % maze_pitch == 0 && x < size)
                if (y == 0 || y == size || !g.is_open((cy-1)*g.w + cx, cy*g.w + cx))
                    m.at(x, y) |= wall_n;
        }
    return m;
}

Array<uint32_t> room_lines(rng& r)
{
    Array<uint32_t> lines;
    arrayAppend(lines, 0u);
    for (;;)
    {
        const auto x = lines.back() + room_min + r(room_spread);
        if (x + room_min > map_tiles)
            break;
        arrayAppend(lines, x);
    }
    arrayAppend(lines, map_tiles);
    return lines;
}

wall_map draw_rooms(rng& r)
{
    const auto xs = room_lines(r), ys = room_lines(r);
    cell_graph g{(uint32_t)xs.size() - 1, (uint32_t)ys.size() - 1};
    wilson(g, r);
    // The spanning tree opens about half the walls. Opening half of the rest leaves doors in 75 %.
    for (uint32_t c = 0; c < g.w*g.h; c++)
    {
        if (c % g.w + 1 < g.w && r(2))
            g.connect(c, c + 1);
        if (c / g.w + 1 < g.h && r(2))
            g.connect(c, c + g.w);
    }

    wall_map m{map_tiles};
    for (uint32_t j = 0; j < g.h; j++)
        for (uint32_t i = 0; i <= g.w; i++)
        {
            const auto y0 = ys[j], len = ys[j+1] - y0;
            const bool door = i > 0 && i < g.w && g.is_open(j*g.w + i - 1, j*g.w + i);
            const auto door_y = door ? y0 + 1 + r(len - 2) : (uint32_t)-1;
            for (auto y = y0; y < y0 + len; y++)
                if (y != door_y)
                    m.at(xs[i], y) |= wall_w;
        }
    for (uint32_t j = 0; j <= g.h; j++)
        for (uint32_t i = 0; i < g.w; i++)
        {
            const auto x0 = xs[i], len = xs[i+1] - x0;
            const bool door = j > 0 && j < g.h && g.is_open((j-1)*g.w + i, j*g.w + i);
            const auto door_x = door ? x0 + 1 + r(len - 2) : (uint32_t)-1;
            for (auto x = x0; x < x0 + len; x++)
                if (x != door_x)
                    m.at(x, ys[j]) |= wall_n;
        }
    return m;
}

struct far_tile { uint32_t tile, dist; };

far_tile farthest(const wall_map& m, uint32_t src)
{
    const auto n = m.size;
    Array<uint32_t> dist{DirectInit, n*n, (uint32_t)-1};
    Array<uint32_t> queue{NoInit, n*n};
    uint32_t head = 0, tail = 0;
    dist[src] = 0;
    queue[tail++] = src;
    far_tile best{src, 0};

    while (head < tail)
    {
        const auto t = queue[head++];
        const auto x = t % n, y = t / n;
        if (dist[t] > best.dist)
            best = {t, dist[t]};
        const auto visit = [&](uint32_t u) {
            if (dist[u] != (uint32_t)-1)
                return;
            dist[u] = dist[t] + 1;
            queue[tail++] = u;
        };
        if (x + 1 < n && !(m.at(x+1, y) & wall_w))
            visit(t + 1);
        if (x > 0 && !(m.at(x, y) & wall_w))
            visit(t - 1);
        if (y + 1 < n && !(m.at(x, y+1) & wall_n))
            visit(t + n);
        if (y > 0 && !(m.at(x, y) & wall_n))
            visit(t - n);
    }
    return best;
}

point tile_point(chunk_coords_ origin, const wall_map& m, uint32_t t)
{
    const auto x = t % m.size, y = t / m.size;
    const auto cx = origin.x + (int)(x / TILE_MAX_DIM), cy = origin.y + (int)(y / TILE_MAX_DIM);
    return { chunk_coords_{(int16_t)cx, (int16_t)cy, 0},
             local_coords{(uint8_t)(x % TILE_MAX_DIM), (uint8_t)(y % TILE_MAX_DIM)}, {} };
}

wall_map make_map(kind k, rng& r)
{
    if (k == kind::rooms)
        return draw_rooms(r);
    cell_graph g{map_tiles/maze_pitch, map_tiles/maze_pitch};
    if (k == kind::braid)
    {
        wilson(g, r);
        braid(g, r);
    }
    else
        prim(g, r);
    return draw_maze(g);
}

// A tile centre's own cell is blocked next to a wall: from other positions in that cell the bbox
// reaches the strip. Dijkstra's goal check reads that cell.
point open_point(world& w, Grid::Pass::Pool& pool, point centre)
{
    pool.maybe_mark_stale_all(w);
    constexpr Vector2i offsets[] = { {0, 0}, {-8, -8}, {-8, 0}, {0, -8}, {-16, -16}, {8, 8}, };
    for (auto off : offsets)
    {
        const auto pt = point::normalize_coords(centre, off);
        auto g = pool[w[pt.chunk3()]];
        g.build_if_stale();
        if (g.bit(g.get_bitmask_index_from_coord(intra_coord{pt})))
            return pt;
    }
    fm_abort("no open pass cell near the tile centre");
}

enum class side : uint8_t { w, e, n, s };

struct opening
{
    uint32_t cell;
    side s;
    Vector2i outside;
};

struct cell_bfs
{
    const cell_graph& g;
    Array<uint32_t> dist, queue;

    explicit cell_bfs(const cell_graph& g): g{g}, dist{NoInit, g.w*g.h}, queue{NoInit, g.w*g.h} {}

    void run(uint32_t src)
    {
        for (auto& d : dist)
            d = (uint32_t)-1;
        uint32_t head = 0, tail = 0;
        dist[src] = 0;
        queue[tail++] = src;
        while (head < tail)
        {
            const auto c = queue[head++];
            const auto nb = g.neighbors(c);
            for (auto i = 0u; i < nb.count; i++)
                if (const auto u = nb.cell[i]; dist[u] == (uint32_t)-1 && g.is_open(c, u))
                {
                    dist[u] = dist[c] + 1;
                    queue[tail++] = u;
                }
        }
    }
};

struct maze_ends
{
    uint32_t entrance;
    opening exit;
    uint32_t dist;
};

// A* with a consistent h expands every node with g + h < C*. Picks the border entrance and exit
// with the most cells under that bound, counted at cell resolution.
maze_ends pick_ends(const cell_graph& g, Search::octile oct)
{
    constexpr auto cell_px = maze_pitch*(uint32_t)tile_size_xy;
    const auto n = g.w*g.h;
    Array<opening> exits;
    arrayReserve(exits, 2*(g.w + g.h));
    for (uint32_t x = 0; x < g.w; x++)
    {
        arrayAppend(exits, opening{x, side::n, {(int)x, -1}});
        arrayAppend(exits, opening{(g.h-1)*g.w + x, side::s, {(int)x, (int)g.h}});
    }
    for (uint32_t y = 0; y < g.h; y++)
    {
        arrayAppend(exits, opening{y*g.w, side::w, {-1, (int)y}});
        arrayAppend(exits, opening{y*g.w + g.w-1, side::e, {(int)g.w, (int)y}});
    }

    cell_bfs bfs{g};
    // A cell under the bound is at most as far from the entrance as the exit cell, so the count
    // of those caps the exit's score and skips the full count for most exits.
    Array<uint32_t> at_most{NoInit, n};
    maze_ends best{};
    uint32_t best_count = 0;
    for (uint32_t s = 0; s < n; s++)
    {
        const auto sx = s % g.w, sy = s / g.w;
        if (sx != 0 && sy != 0 && sx != g.w-1 && sy != g.h-1)
            continue;
        bfs.run(s);
        for (auto& k : at_most)
            k = 0;
        for (auto d : bfs.dist)
            at_most[d]++;
        for (auto k = 1u; k < n; k++)
            at_most[k] += at_most[k-1];

        for (const auto& e : exits)
        {
            const auto de = bfs.dist[e.cell];
            if (e.cell == s || at_most[de] < best_count)
                continue;
            const auto C = (de + 1)*cell_px;
            uint32_t count = 0;
            for (uint32_t y = 0, c = 0; y < g.h; y++)
                for (uint32_t x = 0; x < g.w; x++, c++)
                {
                    const auto delta = (Vector2i{(int)x, (int)y} - e.outside)*(int)cell_px;
                    count += bfs.dist[c]*cell_px + oct(delta) < C;
                }
            if (count > best_count || (count == best_count && de > best.dist))
            {
                best_count = count;
                best = {s, e, de};
            }
        }
    }
    return best;
}

} // namespace

StringView name(kind k)
{
    switch (k)
    {
    case kind::rooms: return "rooms"_s;
    case kind::braid: return "braid"_s;
    case kind::prim: return "prim"_s;
    case kind::COUNT: break;
    }
    fm_abort("mazes: bad kind %d", (int)k);
}

route build(world& w, kind k, chunk_coords_ origin, Grid::Pass::Pool& pool)
{
    fm_assert(origin.z == 0);
    fm_assert(pool.params().bbox_size == bbox_size);
    auto r = rng{0x5eed0000u + (uint32_t)k};
    const auto m = make_map(k, r);
    const auto ground = loader.ground_atlas("floor-tiles");
    const auto wall = wall_image_proto{loader.wall_atlas("empty", loader_policy::warn), 0};

    for (int16_t cy = 0; cy < map_chunks; cy++)
        for (int16_t cx = 0; cx < map_chunks; cx++)
        {
            auto& c = w[chunk_coords_{(int16_t)(origin.x + cx), (int16_t)(origin.y + cy), 0}];
            for (uint8_t ly = 0; ly < TILE_MAX_DIM; ly++)
                for (uint8_t lx = 0; lx < TILE_MAX_DIM; lx++)
                {
                    auto t = c[local_coords{lx, ly}];
                    t.ground() = { ground, variant_t((lx + ly) % ground->num_tiles()) };
                    const auto x = (uint32_t)cx*TILE_MAX_DIM + lx, y = (uint32_t)cy*TILE_MAX_DIM + ly;
                    if (x > m.size || y > m.size)
                        continue;
                    const auto bits = m.at(x, y);
                    if (bits & wall_w)
                        t.wall_west() = wall;
                    if (bits & wall_n)
                        t.wall_north() = wall;
                }
            c.mark_modified();
            c.ensure_passability();
        }

    const auto a = farthest(m, 0), b = farthest(m, a.tile);
    const auto from = open_point(w, pool, tile_point(origin, m, a.tile));
    const auto to = open_point(w, pool, tile_point(origin, m, b.tile));

    const auto oct = Search::octile{pool.params().div_size()};
    const auto h_max = oct(tile_point(origin, m, 0), tile_point(origin, m, m.size*m.size - 1)) + 2*tile_size_xy;
    // If the walk through the BFS path's tile centres is passable, the optimum costs at most
    // b.dist*tile_size_xy. A pushed node's g is then at most that plus one step and its h at most
    // h_max, so this max_dist prunes nothing. A larger one only grows Search::cache's rectangle.
    const auto max_dist = b.dist*tile_size_xy + h_max + 2*tile_size_xy;
    return { from, to, max_dist, b.dist };
}

route replace(world& w, chunk_coords_ origin, uint32_t cells, uint64_t seed,
              Grid::Pass::Pool& pool, object_id keep)
{
    fm_assert(cells >= 2);
    constexpr auto pitch = (int)maze_pitch;
    auto r = rng{seed};
    cell_graph g{cells, cells};
    growing_tree(g, r);
    const auto oct = Search::octile{pool.params().div_size()};
    const auto ends = pick_ends(g, oct);
    const auto& ex = ends.exit;
    auto m = draw_maze(g);
    {
        const auto x0 = (ex.cell % g.w)*maze_pitch, y0 = (ex.cell / g.w)*maze_pitch;
        for (auto k = 0u; k < maze_pitch; k++)
            switch (ex.s)
            {
            case side::w: m.at(0, y0 + k) &= (uint8_t)~wall_w; break;
            case side::e: m.at(m.size, y0 + k) &= (uint8_t)~wall_w; break;
            case side::n: m.at(x0 + k, 0) &= (uint8_t)~wall_n; break;
            case side::s: m.at(x0 + k, m.size) &= (uint8_t)~wall_n; break;
            }
    }

    const Range2Di maze_rect{{}, Vector2i{(int)m.size + 1}};
    const auto square = Range2Di::fromSize(ex.outside*pitch, Vector2i{pitch});
    const auto tiles = Math::join(maze_rect, square);
    const auto origin_pt = point{origin, local_coords{}, {}};
    const auto c0 = floor_divmod<TILE_MAX_DIM>(tiles.min()).first();
    const auto c1 = floor_divmod<TILE_MAX_DIM>(tiles.max() - Vector2i{1}).first();
    const auto chunk_at = [&](int cx, int cy) {
        return chunk_coords_{(int16_t)(origin.x + cx), (int16_t)(origin.y + cy), origin.z};
    };

    {
        const auto to_px = [](Range2Di t) {
            return Range2Di{t.min()*tile_size_xy - half_tile<Vector2i>, t.max()*tile_size_xy - half_tile<Vector2i>};
        };
        const Range2Di rects[] = { to_px(maze_rect), to_px(square) };
        const auto in_footprint = [&](const object& o) {
            const auto pos = o.position() - origin_pt;
            const auto bb = Range2Di::fromSize(pos + Vector2i{o.bbox_offset} - Vector2i{o.bbox_size}/2,
                                               Vector2i{o.bbox_size});
            for (const auto& px : rects)
                if (px.contains(pos) || Math::intersects(px, bb))
                    return true;
            return false;
        };
        // One chunk further out for bboxes reaching in from a neighbour.
        for (auto cy = c0.y() - 1; cy <= c1.y() + 1; cy++)
            for (auto cx = c0.x() - 1; cx <= c1.x() + 1; cx++)
                if (auto* c = w.at(chunk_at(cx, cy)))
                    // Backwards because arrayRemove() shifts the tail down.
                    c->objects().visit_lists([&]<typename T>(object_list<T>& l) {
                        for (auto i = l.size(); i-- > 0; )
                            if (const object& o = l[i]; o.id != keep && in_footprint(o))
                                c->kill_object(o, i);
                    });
    }

    const auto ground = loader.ground_atlas("floor-tiles");
    const auto wall = wall_image_proto{loader.wall_atlas("concrete1", loader_policy::warn), 0};
    for (auto cy = c0.y(); cy <= c1.y(); cy++)
        for (auto cx = c0.x(); cx <= c1.x(); cx++)
        {
            const auto base = Vector2i{cx, cy}*(int)TILE_MAX_DIM;
            const auto chunk_rect = Range2Di::fromSize(base, Vector2i{(int)TILE_MAX_DIM});
            if (!Math::intersects(chunk_rect, maze_rect) && !Math::intersects(chunk_rect, square))
                continue;
            auto& c = w[chunk_at(cx, cy)];
            for (uint8_t ly = 0; ly < TILE_MAX_DIM; ly++)
                for (uint8_t lx = 0; lx < TILE_MAX_DIM; lx++)
                {
                    const auto t = base + Vector2i{lx, ly};
                    const bool in_maze = maze_rect.contains(t);
                    if (!in_maze && !square.contains(t))
                        continue;
                    auto tile = c[local_coords{lx, ly}];
                    tile.ground() = { ground, variant_t((lx + ly) % ground->num_tiles()) };
                    tile.wall_north() = {};
                    tile.wall_west() = {};
                    if (!in_maze)
                        continue;
                    const auto bits = m.at((uint32_t)t.x(), (uint32_t)t.y());
                    if (bits & wall_w)
                        tile.wall_west() = wall;
                    if (bits & wall_n)
                        tile.wall_north() = wall;
                }
            c.mark_modified();
            c.ensure_passability();
        }

    const auto cell_point = [&](Vector2i cell) {
        return point::normalize_coords(origin_pt, cell*pitch*tile_size_xy + Vector2i{pitch*tile_size_xy/2 - half_tile<int>});
    };
    const auto entrance = Vector2i{(int)(ends.entrance % g.w), (int)(ends.entrance / g.w)};
    const auto from = open_point(w, pool, cell_point(entrance));
    const auto to = open_point(w, pool, cell_point(ex.outside));

    // Same bound as build(), over the footprint's diagonal.
    const auto route_tiles = (ends.dist + 1)*maze_pitch;
    const auto h_max = oct(Vector2i{(int)m.size + 1 + 2*pitch}*tile_size_xy) + 2*tile_size_xy;
    const auto max_dist = route_tiles*tile_size_xy + h_max + 2*tile_size_xy;
    return { from, to, max_dist, route_tiles };
}

} // namespace floormat::mazes
