#include "mazes.hpp"
#include "src/world.hpp"
#include "src/tile-image.hpp"
#include "src/ground-atlas.hpp"
#include "src/grid-pass.hpp"
#include "src/search-pred.hpp"
#include "src/point.inl"
#include "src/intra-coord.inl"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
#include <utility>
#include <cr/Array.h>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>

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
    pool.maybe_mark_stale_all(w.frame_no());
    constexpr Vector2i offsets[] = { {0, 0}, {-8, -8}, {-8, 0}, {0, -8}, {-16, -16}, {8, 8}, };
    for (auto off : offsets)
    {
        const auto pt = point::normalize_coords(centre, off);
        auto g = pool[w[pt.chunk3()]];
        g.build_if_stale(Search::without_critters());
        if (g.bit(g.get_bitmask_index_from_coord(intra_coord{pt})))
            return pt;
    }
    fm_abort("no open pass cell near the tile centre");
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

    const auto& h = Search::octile_distance();
    const auto h_max = h(tile_point(origin, m, 0), tile_point(origin, m, m.size*m.size - 1)) + 2*tile_size_xy;
    // If the walk through the BFS path's tile centres is passable, the optimum costs at most
    // b.dist*tile_size_xy. A pushed node's g is then at most that plus one step and its h at most
    // h_max, so this max_dist prunes nothing. A larger one only grows Search::cache's rectangle.
    const auto max_dist = b.dist*tile_size_xy + h_max + 2*tile_size_xy;
    return { from, to, max_dist, b.dist };
}

} // namespace floormat::mazes
