#include "app.hpp"
#include "compat/any-of.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/debug.hpp"
#include "compat/enumerate.hpp"
#include "compat/function-view.hpp"
#include "compat/nanosecond.hpp"
#include "compat/valgrind.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-object.hpp"
#include "src/grid-registry.hpp"
#include "src/anim-atlas.hpp"
#include "src/critter.hpp"
#include "src/intra-coord.inl"
#include "src/point.inl"
#include "src/raycast.hpp"
#include "src/hole.hpp"
#include "src/RTree.hpp"
#include "src/search.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/scenery-proto.hpp"
#include "src/scenery.hpp"
#include "src/tile-defs.hpp"
#include "src/tile-bbox.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/hwy.hpp"
#include <mg/Functions.h>
#include <mg/Range.h>
#include <hwy/targets.h>
#include <algorithm>
#include <bit>

namespace floormat::Test {

namespace {

constexpr chunk_coords_ COORD{0, 0, 0};
constexpr chunk_coords_ COORD_E{1, 0, 0};
constexpr chunk_coords_ COORD_Eʹ{0, 1, 0};

tile_image_proto floor_proto()
{
    return tile_image_proto{ loader.ground_atlas("tiles"), 0 };
}

wall_image_proto wall_proto()
{
    return wall_image_proto{ loader.wall_atlas("empty"), 0 };
}

void add_ground_all(chunk& c)
{
    const auto floor = floor_proto();
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = floor;
}

void clear_ground_all(chunk& c)
{
    const auto empty = tile_image_proto{};
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = empty;
}

void add_wall_north(chunk& c, local_coords pos)
{
    c[pos].wall_north() = wall_proto();
}

void rebuild_passability(chunk& c)
{
    c.mark_passability_modified();
    c.ensure_passability();
}

void tick(world&, Pass::Pool& pool)
{
    pool.build_if_stale_all();
}

u32 count_passable(const Pass::Grid& g)
{
    const auto dc = g.div_count();
    u32 n = 0;
    for (auto j = 0u; j < dc; j++)
        for (auto i = 0u; i < dc; i++)
            if (g.bit(Pass::Grid::get_bitmask_index(i, j, dc)))
                n++;
    return n;
}

u32 count_passable_built(Pass::Pool& pool, chunk& c)
{
    Pass::Grid g = pool[c];
    g.build_if_stale();
    return count_passable(g);
}

template<typename Pool>
struct far_grid
{
    world& w;
    Pool& pool;
    chunk_coords_ ch;
    u64 build_no;

    far_grid(world& w, Pool& pool, chunk_coords_ changed, i32 dist):
        w{w}, pool{pool}, ch{changed + Vector2i{dist, 0}}
    {
        auto& c = w[ch];
        add_ground_all(c);
        rebuild_passability(c);
        auto g = pool[c];
        g.build_if_stale();
        build_no = g.build_no();
    }

    void check(const char* what) const
    {
        auto g = pool[*w.at(ch)];
        if (g.is_marked_stale() || g.is_stale())
        {
            ERR << "!!! fatal: grid of chunk" << (int)ch.x << (int)ch.y << "stale after" << what;
            fm_abort("a change two or more chunks away made a grid stale");
        }
        g.build_if_stale();
        fm_assert(g.build_no() == build_no);
    }
};

void test_ground_only_is_fully_passable(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);

    Pass::Grid grid = pool[c];
    grid.build_if_stale();

    const auto dc = grid.div_count();
    fm_assert(dc == chunk_size_xy / div_size);
    fm_assert(count_passable(grid) == dc * dc);
}

void test_wall_blocks_some_cells(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    {
        Pass::Grid grid = pool[c];
        grid.build_if_stale();
        const auto dc = grid.div_count();
        fm_assert(count_passable(grid) == dc * dc);
    }
    const far_grid far{w, pool, COORD, 2};

    add_wall_north(c, {8, 8});
    rebuild_passability(c);
    far.check("a wall");

    tick(w, pool);
    {
        Pass::Grid grid = pool[c];
        grid.build_if_stale();
        const auto dc = grid.div_count();
        const auto after = count_passable(grid);
        fm_assert(after < dc * dc);
        fm_assert(after > 0);
    }
}

void test_idempotent_rebuild(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid grid = pool[c];
    grid.build_if_stale();
    const auto first = count_passable(grid);

    grid.mark_stale();
    grid.build_if_stale();
    const auto second = count_passable(grid);
    fm_assert(first == second);
}

void test_neighbor_cascade(u32 div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g0 = pool[c0];
    Pass::Grid g1 = pool[c1];
    g0.build_if_stale();
    g1.build_if_stale();
    const auto base1 = count_passable(g1);
    const far_grid far{w, pool, COORD_E, 2};

    add_wall_north(c1, {0, 1});
    rebuild_passability(c1);
    far.check("a wall");

    tick(w, pool);
    Pass::Grid g1ʹ = pool[c1];
    const auto after1 = count_passable(g1ʹ);
    fm_assert(after1 < base1);
}

void test_collect_cleanup(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    {
        Pass::Grid grid = pool[c];
        grid.build_if_stale();
        fm_assert(count_passable(grid) > 0);
    }

    fm_assert(w.at(COORD) != nullptr);

    clear_ground_all(c);
    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    tick(w, pool);
}

void test_add_remove_wall_restores_baseline(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    const auto baseline = count_passable_built(pool, c);
    const far_grid far{w, pool, COORD, 2};

    add_wall_north(c, {8, 8});
    rebuild_passability(c);
    far.check("adding a wall");
    tick(w, pool);
    const auto with_wall = count_passable_built(pool, c);
    fm_assert(with_wall < baseline);

    c[{8, 8}].wall_north() = wall_image_proto{};
    rebuild_passability(c);
    far.check("removing a wall");
    tick(w, pool);
    const auto restored = count_passable_built(pool, c);
    fm_assert(restored == baseline);
}

void test_neighbor_west_wall_affects_east_edge(u32 div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    const auto base0 = count_passable_built(pool, c0);
    const far_grid far{w, pool, COORD_E, 2};

    for (auto y = 0u; y < TILE_MAX_DIM; y++)
        c1[{0, (u8)y}].wall_west() = wall_proto();
    rebuild_passability(c1);
    far.check("a west wall");

    tick(w, pool);
    const auto after0 = count_passable_built(pool, c0);
    fm_assert(after0 < base0);
}

void test_bit_from_tile_center_passable(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid grid = pool[c];
    grid.build_if_stale();

    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
        {
            const auto idx = grid.get_bitmask_index_from_coord({local_coords{(u8)i, (u8)j}, {}});
            fm_assert(grid.bit(idx));
        }
}

void test_wall_then_clear_ground_allows_collect(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    { const auto passable = count_passable_built(pool, c);
      fm_assert(passable > 0); }

    clear_ground_all(c);
    c[{4, 4}].wall_north() = wall_image_proto{};
    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    tick(w, pool);
}

void test_cell_at_wall_is_blocked(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {8, 8});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto idx = g.get_bitmask_index_from_coord({local_coords{8, 7}, Vector2b{0, 28}});
    fm_assert(!g.bit(idx));
}

void test_cell_south_of_wall_is_passable(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {8, 8});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto idx = g.get_bitmask_index_from_coord({local_coords{8, 10}, {}});
    fm_assert(g.bit(idx));
}

void test_all_chunk_corners_passable(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const local_coords corners[] = { {0, 0}, {15, 0}, {0, 15}, {15, 15} };
    for (const auto lc : corners)
    {
        const auto idx = g.get_bitmask_index_from_coord({lc, {}});
        fm_assert(g.bit(idx));
    }
}

void test_div_count_derived_from_div_size(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    fm_assert(g.div_count() == chunk_size_xy / div_size);
    fm_assert(g.div_count() * div_size == chunk_size_xy);
}

void test_pool_destruction_with_live_grids()
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    {
        Pass::Pool pool{Pass::Params{16, pass_mode::pass}};
        tick(w, pool);
        Pass::Grid g = pool[c];
        g.build_if_stale();
        fm_assert(count_passable(g) > 0);
    }
}

void add_blocked_ground_all(chunk& c)
{
    const auto blocker = tile_image_proto{ loader.ground_atlas("texel"), 0 };
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = blocker;
}

void test_blocked_ground_blocks_all(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_blocked_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();
    fm_assert(count_passable(g) == 0);
}

void test_multi_chunk_3x3_sanity(u32 div_size)
{
    auto w = world();
    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = w[{(i16)x, (i16)y, 0}];
            add_ground_all(c);
            rebuild_passability(c);
        }

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = *w.at(chunk_coords_{(i16)x, (i16)y, 0});
            Pass::Grid g = pool[c];
            g.build_if_stale();
            const auto dc = g.div_count();
            fm_assert(count_passable(g) == dc * dc);
        }
}

void test_chunk_reinsertion_after_collect(u32 div_size)
{
    auto w = world();

    {
        auto& c = w[COORD];
        add_ground_all(c);
        rebuild_passability(c);
    }

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    { Pass::Grid g = pool[*w.at(COORD)];
      g.build_if_stale();
      fm_assert(count_passable(g) > 0); }
    const far_grid far{w, pool, COORD, 2};

    clear_ground_all(*w.at(COORD));
    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);
    far.check("collecting");

    tick(w, pool);

    auto& c2 = w[COORD];
    add_ground_all(c2);
    rebuild_passability(c2);
    far.check("reinserting");

    tick(w, pool);
    Pass::Grid g2 = pool[c2];
    g2.build_if_stale();
    const auto dc = g2.div_count();
    fm_assert(count_passable(g2) == dc * dc);
}

void test_chunk_pass_gen_unique_after_collect()
{
    auto w = world();

    auto id1 = w[COORD].pass_gen();
    fm_assert(id1 != 0);

    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    auto id2 = w[COORD].pass_gen();
    fm_assert(id2 != 0);
    fm_assert(id1 != id2);
}

void make_chunk_row(world& w, i16 n)
{
    for (i16 i = 0; i < n; i++)
        w[chunk_coords_{i, 0, 0}];
}

u64 max_pass_gen(world& w)
{
    u64 ret = 0;
    for (const auto& c : w.chunks())
        ret = Math::max(ret, c.pass_gen());
    return ret;
}

void test_chunk_pass_gen_monotonic_after_move_ctor()
{
    auto a = world();
    make_chunk_row(a, 20);
    auto b = world(move(a));
    const auto old = max_pass_gen(b);
    auto& c = b[COORD];
    c.mark_passability_modified();
    fm_assert(c.pass_gen() > old);
}

void test_chunk_pass_gen_monotonic_after_move_assign()
{
    auto a = world();
    make_chunk_row(a, 20);
    auto b = world();
    b[COORD];
    b = move(a);
    const auto old = max_pass_gen(b);
    auto& c = b[COORD];
    c.mark_passability_modified();
    fm_assert(c.pass_gen() > old);
}

void test_grids_after_world_move_ctor()
{
    Pass::Pool pool{Pass::Params{16, pass_mode::pass}};
    auto a = world();
    u64 build_no;
    {
        auto& c = a[COORD];
        add_ground_all(c);
        rebuild_passability(c);
        tick(a, pool);
        auto g = pool[c];
        g.build_if_stale();
        build_no = g.build_no();
    }
    auto b = world(move(a));
    tick(b, pool);
    Pass::Grid g = pool[b[COORD]];
    fm_assert(g.build_no() == build_no);
    const auto dc = g.div_count();
    fm_assert(count_passable(g) == dc * dc);
}

void test_world_move_ctor_from_moved_from()
{
    auto a = world();
    make_chunk_row(a, 3);
    auto b = world(move(a));
    auto c = world(move(a));
    fm_assert(b.contains(chunk_coords_{2, 0, 0}));
}

void test_grids_dropped_by_world_move_assign()
{
    Pass::Pool pool{Pass::Params{16, pass_mode::pass}};
    auto a = world();
    a[COORD];
    auto b = world();
    {
        auto& c = b[chunk_coords_{5, 5, 0}];
        add_ground_all(c);
        rebuild_passability(c);
        tick(b, pool);
        pool[c].build_if_stale();
    }
    fm_assert(pool.free_grid_count() == 0);
    b = move(a);
    fm_assert(pool.free_grid_count() == 1);
    tick(b, pool);
}

void test_world_assign_into_moved_from()
{
    Pass::Pool pool{Pass::Params{16, pass_mode::pass}};
    auto a = world();
    make_chunk_row(a, 3);
    auto b = world(move(a));
    a = move(b);
    fm_assert(a.contains(chunk_coords_{2, 0, 0}));
    a[COORD];
    tick(a, pool);
    b = world();
    b[COORD];
    tick(b, pool);
}

void test_frame_counter_ticks_independently(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    { Pass::Grid g = pool[c];
      g.build_if_stale();
      (void)count_passable(g); }

    (void)w.increment_frame_no();
    tick(w, pool);

    Pass::Grid g = pool[c];
    g.build_if_stale();
    const auto dc = g.div_count();
    fm_assert(count_passable(g) == dc * dc);
}

void test_empty_pool_safe_tick(u32 div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    tick(w, pool);
}

void test_last_bit_index_valid(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto dc = g.div_count();
    const auto last_idx = Pass::Grid::get_bitmask_index(dc - 1, dc - 1, dc);
    fm_assert(g.bit(last_idx));
}

void test_wall_west_blocks(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    c[{8, 8}].wall_west() = wall_proto();
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto dc = g.div_count();
    fm_assert(count_passable(g) < dc * dc);
    fm_assert(count_passable(g) > 0);
}

void test_many_walls_strictly_reduce_passable(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);
    const far_grid far{w, pool, COORD, 2};

    add_wall_north(c, {4, 4});
    rebuild_passability(c);
    far.check("the first wall");
    tick(w, pool);
    const auto one = count_passable_built(pool, c);
    fm_assert(one < base);

    add_wall_north(c, {8, 8});
    rebuild_passability(c);
    far.check("the second wall");
    tick(w, pool);
    const auto two = count_passable_built(pool, c);
    fm_assert(two < one);
}

void test_repeated_ticks_noop_on_stable_world(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {5, 5});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    const auto first = count_passable_built(pool, c);

    for (int i = 0; i < 5; i++)
    {
        tick(w, pool);
        fm_assert(count_passable_built(pool, c) == first);
    }
}

void test_explicit_wrapper_mark_stale_rebuilds(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    const far_grid far{w, pool, COORD, 2};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);

    Pass::Grid g = pool[c];
    const auto pass_gen = g.built_pass_gen();
    fm_assert(!g.is_stale());
    fm_assert(!g.is_stale(pass_gen));
    g.mark_stale();
    far.check("mark_stale()");
    fm_assert(g.is_stale());
    fm_assert(g.is_stale(pass_gen));
    g.build_if_stale();
    fm_assert(!g.is_stale());
    fm_assert(!g.is_stale(pass_gen));
    fm_assert(count_passable(g) == base);
}

void test_mark_modified_only_without_ensure_passability(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);
    const auto g = pool[c];
    const auto pass_gen = g.built_pass_gen();
    const far_grid far{w, pool, COORD, 2};

    add_wall_north(c, {8, 8});
    c.mark_passability_modified();
    far.check("mark_passability_modified()");
    fm_assert(g.is_stale());
    fm_assert(g.is_stale(pass_gen));

    tick(w, pool);
    const auto after = count_passable_built(pool, c);
    fm_assert(!g.is_stale());
    fm_assert(g.is_stale(pass_gen));
    fm_assert(!g.is_stale(g.built_pass_gen()));
    fm_assert(after < base);
}

void test_neighbor_mark_only_without_ensure_detected(u32 div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    const auto base0 = count_passable_built(pool, c0);
    const auto g0 = pool[c0];
    const auto pass_gen = g0.built_pass_gen();
    const far_grid far{w, pool, COORD_E, 2};

    for (auto y = 0u; y < TILE_MAX_DIM; y++)
        c1[{0, (u8)y}].wall_west() = wall_proto();
    c1.mark_passability_modified();
    far.check("mark_passability_modified()");
    fm_assert(g0.is_stale());
    fm_assert(g0.is_stale(pass_gen));

    tick(w, pool);
    const auto after0 = count_passable_built(pool, c0);
    fm_assert(!g0.is_stale());
    fm_assert(g0.is_stale(pass_gen));
    fm_assert(!g0.is_stale(g0.built_pass_gen()));
    fm_assert(after0 < base0);
}

// built_pass_gen() is world::pass_gen() at the rebuild, so a rebuild after an edit anywhere in the world
// changes it, although nothing the grid reads changed.
void test_rebuild_after_far_change_reads_as_stale(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    Pass::Grid g = pool[c];
    tick(w, pool);
    const auto pass_gen = g.built_pass_gen();

    g.mark_stale();
    g.build_if_stale();
    fm_assert(g.built_pass_gen() == pass_gen);
    fm_assert(!g.is_stale(pass_gen));

    (void)w[chunk_coords_{8, 8, 0}];
    fm_assert(!g.is_stale());
    g.mark_stale();
    g.build_if_stale();
    fm_assert(g.built_pass_gen() > pass_gen);
    fm_assert(!g.is_stale());
    fm_assert(g.is_stale(pass_gen));
}

void test_params_preserved_through_pool(u32 div_size)
{
    Pass::Pool pool{Pass::Params{div_size, tile_size_xy, pass_mode::pass}};
    const auto p = pool.params();
    fm_assert(p.div_size() == div_size);
    fm_assert(p.bbox_size == tile_size_xy);
}

void test_collect_all_chunks_no_crash(u32 div_size)
{
    auto w = world();
    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = w[{(i16)x, (i16)y, 0}];
            add_ground_all(c);
            rebuild_passability(c);
        }

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
            { auto& c = *w.at(chunk_coords_{(i16)x, (i16)y, 0});
              (void)count_passable_built(pool, c); }

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
            clear_ground_all(*w.at(chunk_coords_{(i16)x, (i16)y, 0}));

    w.collect(true, true);
    fm_assert(w.size() == 0);

    tick(w, pool);
}

void test_pool_reuse_after_collect(u32 div_size)
{
    constexpr chunk_coords_ C11{1, 1, 0};
    auto w = world();
    auto& c = w[C11];
    add_wall_north(c, {0, 0});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    { Pass::Grid g = pool[c]; g.build_if_stale(); }

    const auto before = pool.free_grid_count();

    c[{0, 0}].wall_north() = wall_image_proto{};
    c.mark_passability_modified();
    w.collect(true, true);
    fm_assert(w.at(C11) == nullptr);

    fm_assert(pool.free_grid_count() == before + 1);
}

void test_pool_reuse_neighbors_collected(u32 div_size)
{
    constexpr chunk_coords_ A{1, 1, 0};
    constexpr chunk_coords_ B{2, 1, 0};
    auto w = world();
    { auto& ca = w[A]; add_wall_north(ca, {0, 0}); rebuild_passability(ca); }
    { auto& cb = w[B]; add_wall_north(cb, {0, 0}); rebuild_passability(cb); }

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    { (void)pool[*w.at(A)]; (void)pool[*w.at(B)]; }

    const auto before = pool.free_grid_count();

    { auto& ca = *w.at(A);
      ca[{0, 0}].wall_north() = wall_image_proto{};
      ca.mark_passability_modified(); }
    { auto& cb = *w.at(B);
      cb[{0, 0}].wall_north() = wall_image_proto{};
      cb.mark_passability_modified(); }
    w.collect(true, true);
    fm_assert(w.at(A) == nullptr);
    fm_assert(w.at(B) == nullptr);

    fm_assert(pool.free_grid_count() == before + 2);
}

void test_neighbor_lifecycle(u32 div_size)
{
    constexpr chunk_coords_ A{0, 0, 0};
    constexpr chunk_coords_ B{1, 0, 0};
    auto w = world();
    { auto& ca = w[A]; add_ground_all(ca); rebuild_passability(ca); }

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);
    const far_grid far{w, pool, B, 2};

    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }
    far.check("creating");
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);

    const auto free_count0 = pool.free_grid_count();
    clear_ground_all(*w.at(B));
    w.collect(true, true);
    fm_assert(w.at(B) == nullptr);
    far.check("collecting");
    tick(w, pool);
    fm_assert(pool.free_grid_count() == free_count0 + 1);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);

    const auto free_count1 = pool.free_grid_count();
    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }
    far.check("recreating");
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);
    fm_assert(pool.free_grid_count() == free_count1 - 1);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);
}

void test_self_collect_with_neighbor(u32 div_size)
{
    constexpr chunk_coords_ A{0, 0, 0};
    constexpr chunk_coords_ B{1, 0, 0};
    auto w = world();
    { auto& ca = w[A]; add_ground_all(ca); rebuild_passability(ca); }
    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(A));
    (void)count_passable_built(pool, *w.at(B));
    const far_grid far{w, pool, A, 2};

    const auto free_count0 = pool.free_grid_count();
    clear_ground_all(*w.at(A));
    w.collect(true, true);
    fm_assert(w.at(A) == nullptr);
    fm_assert(w.at(B) != nullptr);
    far.check("collecting");

    tick(w, pool);
    fm_assert(pool.free_grid_count() == free_count0 + 1);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);
}

void test_pool_reuse_across_different_coord(u32 div_size)
{
    constexpr chunk_coords_ X{0, 0, 0};
    constexpr chunk_coords_ Y{3, 3, 0};
    auto w = world();
    { auto& c = w[X]; add_ground_all(c); rebuild_passability(c); }

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(X));

    clear_ground_all(*w.at(X));
    w.collect(true, true);
    tick(w, pool);
    const auto free_count_after_collect = pool.free_grid_count();
    fm_assert(free_count_after_collect >= 1);

    { auto& c = w[Y]; add_ground_all(c); rebuild_passability(c); }
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(Y));
    fm_assert(pool.free_grid_count() == free_count_after_collect - 1);
}

void test_mark_and_ensure_preserve_frame_no(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    const auto frame = w.frame_no();

    add_wall_north(c, {4, 4});
    c.mark_passability_modified();
    fm_assert(w.frame_no() == frame);
    c.ensure_passability();
    fm_assert(w.frame_no() == frame);

    tick(w, pool);
    fm_assert(w.frame_no() == frame);

    Pass::Grid g = pool[c];
    g.build_if_stale();
    fm_assert(w.frame_no() == frame);
}

void test_multiple_free_grids(u32 div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    {
        auto& c = w[COORD];
        tick(w, pool);
        fm_assert(pool.free_grid_count() == 0);
        (void)pool[c];
        fm_assert(pool.free_grid_count() == 0);
    }
    w.collect(true, true); // invalidates c
    tick(w, pool);
    fm_assert(pool.free_grid_count() == 1);
    {
        auto& c = w[COORD];
        (void)pool[c];
        auto& cʹ = w[COORD_E];
        (void)pool[cʹ];
        fm_assert(pool.free_grid_count() == 0);
    }
    w.collect(true, true); // invalidates c, cʹ
    tick(w, pool);
    fm_assert(pool.free_grid_count() == 2);
    {
        auto& c = w[COORD];
        (void)pool[c];
        fm_assert(pool.free_grid_count() == 1);
        auto& cʹ = w[COORD_E];
        (void)pool[cʹ];
        fm_assert(pool.free_grid_count() == 0);
        auto& cʹʹ = w[COORD_Eʹ];
        (void)pool[cʹʹ];
        fm_assert(pool.free_grid_count() == 0);
    }
    w.collect(true, true); // invalidates c, cʹ, cʹʹ
    tick(w, pool);
    fm_assert(pool.free_grid_count() == 3);
}

bool pass_bit(const usize* words, u32 n)
{
    return words[n / (sizeof(usize)*8)] >> n % (sizeof(usize)*8) & 1;
}

void test_bitview_read_matches_bit(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    add_wall_north(c, {8, 8});
    c[{2, 6}].wall_west() = wall_proto();
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto dc = g.div_count();
    const auto view = g.bits();
    for (u32 j = 0; j < dc; j++)
        for (u32 i = 0; i < dc; i++)
        {
            const auto idx = Pass::Grid::get_bitmask_index(i, j, dc);
            fm_assert(view.read(idx) == g.bit(idx));
            fm_assert(view.read(idx) == pass_bit(view.data, idx));
        }

    auto words = Array<usize>{ValueInit, Grid::detail::bitmask_size(dc) / (sizeof(usize)*8)};
    Pass::BitView v{words.data()};
    for (u32 idx = 0; idx < dc*dc; idx++)
    {
        v.set(idx);
        fm_assert(v.read(idx) && pass_bit(words.data(), idx));
        v.write(idx, false);
        fm_assert(!pass_bit(words.data(), idx));
        v.write(idx, true);
        fm_assert(pass_bit(words.data(), idx));
        v.reset(idx);
        fm_assert(!v.read(idx) && !pass_bit(words.data(), idx));
    }
    for (auto x : words)
        fm_assert(x == 0);
}

void test_partial_collect_then_collect_survivors(u32 div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};

    {
        auto& a = w[COORD];
        auto& b = w[COORD_E];
        auto& d = w[COORD_Eʹ];
        add_ground_all(a); add_ground_all(b); add_ground_all(d);
        rebuild_passability(a); rebuild_passability(b); rebuild_passability(d);
        tick(w, pool);
        (void)pool[a]; (void)pool[b]; (void)pool[d];
        clear_ground_all(b);
    }
    w.collect(true, true);
    fm_assert(w.at(COORD)    != nullptr);
    fm_assert(w.at(COORD_E)  == nullptr);
    fm_assert(w.at(COORD_Eʹ) != nullptr);

    tick(w, pool);
    fm_assert(pool.free_grid_count() == 1);

    {
        auto& b = w[COORD_E];
        add_ground_all(b);
        rebuild_passability(b);
        tick(w, pool);
        (void)pool[b];
    }
    fm_assert(pool.free_grid_count() == 0);

    {
        clear_ground_all(*w.at(COORD));
        clear_ground_all(*w.at(COORD_Eʹ));
    }
    w.collect(true, true);
    fm_assert(w.at(COORD)    == nullptr);
    fm_assert(w.at(COORD_E)  != nullptr);
    fm_assert(w.at(COORD_Eʹ) == nullptr);

    tick(w, pool);
    fm_assert(pool.free_grid_count() == 2);
}

// intra_coord adds half_tile and pack_bit_index_from_coord() floors, so cell i holds exactly the
// integer positions [i*div_size - half_tile, (i+1)*div_size - half_tile - 1]. build_impl() inverts this.
void test_cell_spans_match_forward_map(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto dc = g.div_count();
    const int d = (int)div_size;

    for (u8 ly = 0; ly < 3; ly++)
        for (u8 lx = 0; lx < 3; lx++)
            for (int oy = -half_tile<int>; oy < half_tile<int>; oy++)
                for (int ox = -half_tile<int>; ox < half_tile<int>; ox++)
                {
                    const int px = lx*tile_size_xy + ox, py = ly*tile_size_xy + oy;
                    const int i = (px + half_tile<int>) / d, j = (py + half_tile<int>) / d;
                    const auto idx = g.get_bitmask_index_from_coord({local_coords{lx, ly},
                                                                     Vector2b{(i8)ox, (i8)oy}});
                    fm_assert(idx == Pass::Grid::get_bitmask_index((u32)i, (u32)j, dc));
                    fm_assert(px >= i*d - half_tile<int> && px < (i+1)*d - half_tile<int>);
                    fm_assert(py >= j*d - half_tile<int> && py < (j+1)*d - half_tile<int>);
                }
}

struct exact_config
{
    u32 div_size, bbox_size;
    Vector2b chunk_delta;
    u8 at_x, at_y;
    Vector2b obstacle_offset;
    u8 obstacle_size;
};

constexpr exact_config exact_configs[] = {
    {  2,  4, { 0,  0},  4,  4, {  2,   2},  2 },  // div_size/2 rounds down to 1, so the reach differs per side
    {  1,  4, { 0,  0},  3,  5, {  0,   1},  4 },  // one position per cell, no reach either way
    {  4,  4, { 0,  0},  7,  2, { -3,   5},  6 },
    {  4,  8, { 0,  0},  2,  9, {  5,  -7},  2 },
    {  8,  8, { 0,  0},  9,  3, { -1,   6},  8 },
    {  2,  5, { 0,  0},  6,  6, {  3,  -2},  4 },  // odd bbox_size reaches a pixel further above its center
    {  2,  5, { 0,  0},  6,  6, {  3,  -2},  5 },  // odd obstacle too
    {  4,  7, { 1,  0},  0,  6, {-32,   3},  9 },  // both odd, reaching back from the east neighbor
    { 16, 16, { 0,  0},  5,  8, { 11,  -9}, 10 },
    {  4,  8, { 1,  0},  0,  6, {-32,   3},  8 },  // reaches back from the east neighbor
    {  2,  4, { 0, -1},  7, 15, {  5,  31},  8 },  // reaches back from the north neighbor
    {  8,  8, { 0,  0},  7,  3, { 31,   0},  8 },  // span straddles cells 63..64, crossing a word
    {  1,  4, { 0,  0},  3,  5, {  0,   0},200 },  // span covers words 1..5, middle words zeroed
    { 32, 32, { 0,  0},  5,  6, {  3,  -2}, 10 },  // two rows per word
    { 64, 64, { 0,  0},  9,  4, { -5,   7}, 12 },  // four rows per word
    {128,128, { 0,  0}, 10, 11, {  2,   3}, 16 },  // the whole grid is one word
    {256,256, { 0,  0},  6,  9, {  0,   0},  8 },  // 16-bit grid, allocation rounded up to a word
};

// Whether Search::is_passable_ holds for a bbox_size box at the node of cell (i, j) when min_pass == pass, or else
// at every integer position of the cell. Colliders of min_pass or above pass, as in a grid of that min_pass.
bool cell_is_passable(chunk& c, const std::array<chunk*, 8>& nbs, int i, int j, int d, u32 bbox_size,
                      const Search::pred& p, pass_mode min_pass = pass_mode::pass)
{
    const auto q = [&](const chunk& ch, collision_data x) {
        return x.pass >= (u64)min_pass ? path_search_continue::pass : p(ch, x);
    };
    const Search::pred pred{q};
    const auto bb_lo = (float)(bbox_size / 2), bb_hi = (float)bbox_size - bb_lo;
    const bool node_only = min_pass == pass_mode::pass;
    const int o = (int)(bbox_size / 2) % d, n = node_only ? 1 : d, s = node_only ? o : 0;
    for (int py = j*d + s - half_tile<int>; py < j*d + s + n - half_tile<int>; py++)
        for (int px = i*d + s - half_tile<int>; px < i*d + s + n - half_tile<int>; px++)
            if (!Search::is_passable_(&c, nbs, Vector2{(float)px - bb_lo, (float)py - bb_lo},
                                      Vector2{(float)px + bb_hi, (float)py + bb_hi}, pred))
                return false;
    return true;
}

void test_bit_exact(const exact_config& cfg, pass_mode min_pass)
{
    const chunk_coords_ obstacle_ch{(i16)cfg.chunk_delta.x(), (i16)cfg.chunk_delta.y(), 0};

    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    auto& oc = w[obstacle_ch];
    add_ground_all(oc);
    {
        scenery_proto p;
        p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
        p.subtype   = generic_scenery_proto{};
        p.offset    = cfg.obstacle_offset;
        p.bbox_size = Vector2ub(cfg.obstacle_size);
        p.pass      = pass_mode::blocked;
        w.make_scenery(w.make_id(), {obstacle_ch, local_coords{cfg.at_x, cfg.at_y}}, move(p));
    }
    rebuild_passability(c);
    rebuild_passability(oc);

    Pass::Pool pool{Pass::Params{cfg.div_size, cfg.bbox_size, min_pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();
    fm_assert(pool.params().div_size() == cfg.div_size);
    fm_assert(pool.params().bbox_size == cfg.bbox_size);

    const auto nbs = w.neighbors(COORD);
    const Search::pred pred{Search::without_critters};
    const int dc = (int)g.div_count(), d = (int)cfg.div_size;

    // scenery_tile(): center - bbox_size/2, then + bbox_size
    const int mx = cfg.chunk_delta.x()*chunk_size<int> + cfg.at_x*tile_size_xy
                   + cfg.obstacle_offset.x() - cfg.obstacle_size/2,
              my = cfg.chunk_delta.y()*chunk_size<int> + cfg.at_y*tile_size_xy
                   + cfg.obstacle_offset.y() - cfg.obstacle_size/2;
    const int Mx = mx + cfg.obstacle_size, My = my + cfg.obstacle_size;
    // The loop tests only the cells whose box can overlap the obstacle, since dc² is 1024² at div_size 1.
    const int pad = (int)cfg.bbox_size + 2*d;
    const int i_lo = Math::max(0, (mx - pad + half_tile<int>) / d), i_hi = Math::min(dc - 1, (Mx + pad + half_tile<int>) / d),
              j_lo = Math::max(0, (my - pad + half_tile<int>) / d), j_hi = Math::min(dc - 1, (My + pad + half_tile<int>) / d);
    fm_assert(i_lo <= i_hi && j_lo <= j_hi);

    u32 cleared = 0;
    for (int j = j_lo; j <= j_hi; j++)
        for (int i = i_lo; i <= i_hi; i++)
        {
            const bool passable = cell_is_passable(c, nbs, i, j, d, cfg.bbox_size, pred, min_pass);
            cleared += !passable;
            const auto idx = Pass::Grid::get_bitmask_index((u32)i, (u32)j, (u32)dc);
            if (g.bit(idx) != passable)
            {
                Error{standard_error()} << "!!! fatal: div_size" << d << "bbox_size" << (int)cfg.bbox_size
                                        << "cell" << i << j << "bit" << (int)g.bit(idx)
                                        << "but cell_is_passable says" << (int)passable;
                fm_assert(false);
            }
        }
    // nothing outside the window may be cleared, and the config has to clear something
    fm_assert(cleared > 0);
    fm_assert((u32)(dc*dc) - count_passable(g) == cleared);
}

// A hole marker lives in the hole tree, never in the collision tree, so over open ground no
// pass_mode may block anything.
void test_hole_marker_never_blocks(u32 div_size, pass_mode hole_pass)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    auto h = w.make_object<hole>(w.make_id(), global_coords{COORD, {8, 8}}, hole_proto{});
    h->set_bbox({}, {}, Vector2ub{48, 32}, hole_pass);
    rebuild_passability(c);

    // "tiles" ground is pass_mode::pass, so the collision tree is empty
    fm_assert(c.rtree()->Count() == 0);
    fm_assert(c.hole_rtree()->Count() == 1);
    constexpr auto ctr = Vector2(tile_size_xy*8);
    fm_assert(Search::is_passable_1(c, ctr - Vector2{8}, ctr + Vector2{8}, Search::without_critters));

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();
    const auto dc = g.div_count();
    if (count_passable(g) != dc*dc)
    {
        Error{standard_error()} << "!!! fatal: div_size" << div_size << "hole pass mode"
                                << (int)hole_pass << "cleared"
                                << dc*dc - count_passable(g) << "of" << dc*dc << "cells";
        fm_assert(false);
    }
}

// The cut happens at rtree build time, so skipping the marker must not close the opening.
void test_hole_still_opens_blocked_ground(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_blocked_ground_all(c);
    auto h = w.make_object<hole>(w.make_id(), global_coords{COORD, {8, 8}}, hole_proto{});
    h->set_bbox({}, {}, Vector2ub{tile_size_xy}, pass_mode::pass);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, pass_mode::pass}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();
    const auto dc = g.div_count(), n = count_passable(g);
    fm_assert(n > 0 && n < dc*dc);
}

void test_cover_east_distance(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {8, 8});
    rebuild_passability(c);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    Grid::Cover::Grid g = pool[c];
    g.build_if_stale();
    fm_assert(g.built_octants() & 1u);

    const auto east_px = [&](Vector2i pos) {
        return (u32)g.distance(g.get_cell_index_from_coord(intra_coord{pos}), 0) * div_size;
    };
    fm_assert_equal(chunk_size_xy, east_px({0, 0}));

    // "empty" walls are 8 deep: the wall spans intra x 512..575, y 504..511
    const auto pos = Vector2i{400, 508};
    const auto center_x = (u32)pos.x() / div_size * div_size + div_size / 2;
    const auto expected = 512 - center_x;
    // octilinear octants count from the cleared cells, up to half the pass grid's bbox short
    const auto slack = 2 * Math::max(div_size, Pass::Grid::min_bbox_size);
    const auto d = east_px(pos);
    fm_assert(d <= expected && d + slack >= expected);

    const auto east = [&](int x) {
        return g.distance(g.get_cell_index_from_coord(intra_coord{Vector2i{x, pos.y()}}), 0);
    };
    fm_assert_equal(Grid::Cover::blocked, east(540));
    auto x = pos.x();
    while (east(x + (int)div_size) != Grid::Cover::blocked)
    {
        x += (int)div_size;
        fm_assert(x < 540);
    }
    fm_assert_equal(u16{1}, east(x));
}

bptr<scenery> add_blocker(world& w, global_coords pos, Vector2b bbox_offset, Vector2ub bbox_size)
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_offset = bbox_offset;
    p.bbox_size   = bbox_size;
    p.pass        = pass_mode::blocked;
    return w.make_scenery(w.make_id(), pos, move(p));
}

// Walls and scenery in the chunk and in six neighbors, so pass bits vary along every octant on both sides of each
// border. The north-east and south-west neighbors stay missing.
void make_cover_scene(world& w)
{
    const auto chunks = { COORD, chunk_coords_{1, 0, 0}, chunk_coords_{-1, 0, 0}, chunk_coords_{0, 1, 0},
                          chunk_coords_{0, -1, 0}, chunk_coords_{1, 1, 0}, chunk_coords_{-1, -1, 0} };
    for (auto ch : chunks)
        add_ground_all(w[ch]);

    const auto blocker = [&](chunk_coords_ ch, u8 i, u8 j) {
        scenery_proto p;
        p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
        p.subtype   = generic_scenery_proto{};
        p.bbox_size = {24, 24};
        p.pass      = pass_mode::blocked;
        w.make_scenery(w.make_id(), {ch, local_coords{i, j}}, move(p));
    };

    auto& c = w[COORD];
    auto& e = w[chunk_coords_{1, 0, 0}];
    auto& s = w[chunk_coords_{0, 1, 0}];
    auto& se = w[chunk_coords_{1, 1, 0}];
    auto& nw = w[chunk_coords_{-1, -1, 0}];
    for (u8 j = 0; j < TILE_MAX_DIM; j++)
        for (u8 i = 0; i < TILE_MAX_DIM; i++)
        {
            if (j == 5 && i != 7 && i != 8)
                c[{i, j}].wall_north() = wall_proto();
            if (i == 10 && j != 3 && j != 11)
                c[{i, j}].wall_west() = wall_proto();
            if ((i*7 + j*3) % 5 == 0)
                blocker(COORD, i, j);
            if (i == 4 && j % 3 != 0)
                e[{i, j}].wall_west() = wall_proto();
            if (j == 6 && i % 4 != 1)
                s[{i, j}].wall_north() = wall_proto();
            if (i == 12 && j % 2 == 0)
                blocker(chunk_coords_{-1, 0, 0}, i, j);
            if ((i*5 + j*3) % 7 == 0)
                blocker(chunk_coords_{1, 1, 0}, i, j);
            if (i == 3 && j % 4 != 2)
                se[{i, j}].wall_west() = wall_proto();
            if (j == 13 && i % 3 != 0)
                nw[{i, j}].wall_north() = wall_proto();
        }

    // Small blockers 709 to 1020 px past the north and west borders. The octants pointing there reach the cap
    // 724 to 1024 px past the border.
    for (u8 j = 0; j < 5; j++)
        for (u8 i = 0; i < TILE_MAX_DIM; i++)
            if ((i + j) % 2 == 0)
            {
                const auto offset_along_border = (i8)((i*37 + j*11) % 48 - 24);
                const auto offset_across_border = (i8)((i*13 + j*29) % 48 - 24);
                add_blocker(w, {chunk_coords_{0, -1, 0}, local_coords{i, j}},
                            {offset_along_border, offset_across_border}, {8, 8});
                add_blocker(w, {chunk_coords_{-1, 0, 0}, local_coords{j, i}},
                            {offset_across_border, offset_along_border}, {8, 8});
            }

    for (auto ch : chunks)
        rebuild_passability(w[ch]);
}

// octant_steps in src/grid-cover.cpp
Vector2i cover_step(u32 k)
{
    constexpr Vector2i quadrant[] = { {1, 0}, {5, 1}, {5, 2}, {3, 2}, {1, 1}, {2, 3}, {2, 5}, {1, 5} };
    auto s = quadrant[k % 8];
    for (auto i = 0u; i < k / 8; i++)
        s = s.perpendicular();
    return s;
}

// direction_for_octant() in src/grid-cover.cpp
Vector2 cover_direction(u32 k)
{
    return Vector2{cover_step(k)}.normalized();
}

// Returns the vector a ray along octant k covers up to the cap. A non-octilinear octant's ray takes whole steps,
// so it passes through the centers of the cells on its line.
Vector2i cover_ray_delta(u32 k)
{
    if (k % 4 == 0)
        return Vector2i{Vector2(cover_direction(k) * (float)chunk_size_xy)};
    const auto s = cover_step(k);
    return s * (i32)Math::ceil(chunk_size_xy / Math::sqrt((double)s.dot()));
}

// Returns the pixels from the cell's center to the first collider along delta, at most chunk_size_xy.
float cover_ray_px(chunk& c, u32 x, u32 y, u32 div_size, Vector2i delta, Pass::Pool& pass_pool)
{
    const auto half = (i32)(div_size / 2);
    const point from{c.coord(), intra_coord{Vector2i{(i32)(x*div_size) + half, (i32)(y*div_size) + half}}};
    const point to = from + delta;
    const auto r = raycast(c.world(), from, to, 0, pass_pool, Search::without_critters);
    const auto px = !r.has_result || r.success ? (float)chunk_size_xy : Vector2(r.collision - from).length();
    return Math::min(px, (float)chunk_size_xy);
}

// fill_octilinear_octant() in src/grid-cover.cpp carries 1/64 px, saturating at UINT16_MAX
constexpr u32 cover_fixed_one = (UINT16_MAX + 1) / chunk_size_xy;

u32 cover_fixed(float px)
{
    return (u32)(px * (float)cover_fixed_one + .5f);
}

u16 cover_units_fixed(u32 fixed, u32 div_size)
{
    const auto unit = div_size * cover_fixed_one;
    return (u16)((Math::min(fixed, (u32)UINT16_MAX) + unit / 2) / unit);
}

u32 cover_units(float px, u32 div_size)
{
    return cover_units_fixed(cover_fixed(px), div_size);
}

// Cells the segment from a cell's center to the center of the cell a step away passes through, ends
// left out, worked out by hand for octants 1, 2 and 3. Both cells at a corner on the segment count:
// (5, 1)'s segment passes through the corner of (2, 0), (3, 0), (2, 1) and (3, 1). Octants 7, 6 and
// 5 swap x and y of 1, 2 and 3, and the other quadrants turn the first one's cells.
Array<Vector2i> cover_crossed_cells(u32 k)
{
    static constexpr Vector2i crossed_1[] = { {1, 0}, {2, 0}, {3, 0}, {2, 1}, {3, 1}, {4, 1} };
    static constexpr Vector2i crossed_2[] = { {1, 0}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {4, 2} };
    static constexpr Vector2i crossed_3[] = { {1, 0}, {1, 1}, {2, 1}, {2, 2} };
    const auto j = k % 8;
    fm_assert(j % 4 != 0);
    const ArrayView<const Vector2i> base = j == 1 || j == 7 ? ArrayView<const Vector2i>{crossed_1}
                                         : j == 2 || j == 6 ? ArrayView<const Vector2i>{crossed_2}
                                                            : ArrayView<const Vector2i>{crossed_3};
    Array<Vector2i> ret{NoInit, base.size()};
    for (auto i = 0u; i < base.size(); i++)
    {
        auto c = j > 4 ? Vector2i{base[i].y(), base[i].x()} : base[i];
        for (auto t = 0u; t < k / 8; t++)
            c = c.perpendicular();
        ret[i] = c;
    }
    return ret;
}

// Returns the pass bit of each cell of ch and its eight neighbors, 3*dc cells to a row. A missing chunk's cells
// are passable.
Array<bool> pass_bits_of_chunk_and_neighbors(world& w, Pass::Pool& pass_pool, chunk_coords_ ch, u32 dc)
{
    const auto width = 3*dc;
    Array<bool> passable{DirectInit, (usize)width*width, true};
    for (auto j = 0u; j < 3; j++)
        for (auto i = 0u; i < 3; i++)
        {
            auto* const c = w.at(ch + Vector2i{(int)i - 1, (int)j - 1});
            if (!c)
                continue;
            Pass::Grid pg = pass_pool[*c];
            pg.build_if_stale();
            fm_assert(pg.div_count() == dc);
            for (auto y = 0u; y < dc; y++)
                for (auto x = 0u; x < dc; x++)
                    passable[(j*dc + y)*width + i*dc + x] = pg.bit(Pass::Grid::get_bitmask_index(x, y, dc));
        }
    return passable;
}

// Returns the distances octant k holds in the middle chunk of `passable`, the cells
// pass_bits_of_chunk_and_neighbors() returned for a grid of div_size.
Array<u16> cover_reference_distances(ArrayView<const bool> passable, u32 div_size, u32 k)
{
    const auto step = cover_step(k);
    const int q = step.x(), p = step.y();
    const int aq = Math::abs(q), ap = Math::abs(p), major = Math::max(aq, ap);
    const double len = Math::sqrt((double)(q*q + p*p));
    const int dc = (int)(chunk_size_xy / div_size);
    fm_assert(passable.size() == (usize)(9*dc*dc));

    const auto is_in_chunks = [dc](int x, int y) { return x >= -dc && x < 2*dc && y >= -dc && y < 2*dc; };
    const auto is_in_middle_chunk = [dc](int x, int y) { return x >= 0 && x < dc && y >= 0 && y < dc; };
    const auto is_passable = [&](int x, int y) {
        return !is_in_chunks(x, y) || passable[(usize)((y + dc)*3*dc + x + dc)];
    };
    // Rows are counted from the edge the octant points at and go negative past it. Octants 0 and 16 count columns.
    const auto row_from_edge = [=](int x, int y) {
        return p ? (p > 0 ? dc - 1 - y : y) : (q > 0 ? dc - 1 - x : x);
    };

    const auto dir = cover_direction(k);
    // src/grid-cover.cpp holds an octilinear octant's step as a float, and R() rounds multiples of it.
    const float step_px = (float)div_size / Math::abs(q ? dir.x() : dir.y());
    const double fixed_per_row = k % 4 == 0 ? (double)step_px * cover_fixed_one
                                            : div_size * len * cover_fixed_one / ap;
    // Returns n rows in 1/cover_fixed_one px, rounded.
    const auto R = [=](int n) { return (i64)Math::floor(n*fixed_per_row + .5); };

    const auto crossed = k % 4 == 0 ? Array<Vector2i>{} : cover_crossed_cells(k);
    const auto crossed_cell_distance = [&](int major_offset) {
        const double major_px = (u32)major_offset * div_size * len / major;
        return Math::min((u32)(major_px*cover_fixed_one + .5), (u32)UINT16_MAX);
    };

    // The cell at which the model stops following a line, as its row_from_edge() and its distance in
    // 1/cover_fixed_one px.
    struct last_cell { u32 fixed; int n; };
    Array<last_cell> last_cells{NoInit, (usize)(dc*dc)};
    Array<u16> ret{NoInit, (usize)(dc*dc)};
    for (int n = 0; n < dc; n++)
        for (int m = 0; m < dc; m++)
        {
            // Rows nearer the edge come first, so a line's next cell in this chunk already has its last cell.
            const int x = p ? m : q > 0 ? dc - 1 - n : n;
            const int y = !p ? m : p > 0 ? dc - 1 - n : n;
            const auto i = (u32)(y*dc + x);
            if (!is_passable(x, y))
            {
                ret[i] = Grid::Cover::blocked;
                continue;
            }
            auto& last = last_cells[i];
            for (int cx = x, cy = y;; cx += q, cy += p)
            {
                int nearest = major + 1;
                for (auto off : crossed)
                    if (!is_passable(cx + off.x(), cy + off.y()))
                        nearest = Math::min(nearest, aq > ap ? Math::abs(off.x()) : Math::abs(off.y()));
                const int next_x = cx + q, next_y = cy + p;
                if (nearest <= major)
                    last = { crossed_cell_distance(nearest), row_from_edge(cx, cy) };
                else if (!is_in_chunks(next_x, next_y))
                    last = { UINT16_MAX, row_from_edge(cx, cy) };
                else if (!is_passable(next_x, next_y))
                    last = { 0, row_from_edge(next_x, next_y) };
                else if (is_in_middle_chunk(next_x, next_y))
                    last = last_cells[(u32)(next_y*dc + next_x)];
                else
                    continue;
                break;
            }
            const auto fixed = Math::min(last.fixed + R(n) - R(last.n), i64{UINT16_MAX});
            ret[i] = cover_units_fixed((u32)fixed, div_size);
        }
    return ret;
}

void check_cover_octants(ArrayView<const bool> passable, Grid::Cover::Grid& g, u32 div_size,
                         ArrayView<const u32> octants)
{
    const auto dc = g.div_count();
    for (auto k : octants)
    {
        (void)g.ensure_octant(k);
        fm_assert(g.built_octants() & 1u << k);
        const auto expected = cover_reference_distances(passable, div_size, k);
        for (auto i = 0u; i < dc*dc; i++)
        {
            // a passable cell is at least a cell away from the first blocked cell on its line
            fm_assert(g.distance(i, k) != 0);
            if (g.distance(i, k) != expected[i])
            {
                Error{standard_error()} << "!!! fatal: div_size" << div_size << "octant" << k
                                        << "cell" << i % dc << i / dc << "distance" << g.distance(i, k)
                                        << "but expected" << expected[i];
                fm_assert(false);
            }
        }
    }
}

void check_cover_octants(world& w, Grid::Cover::Grid& g, u32 div_size, ArrayView<const u32> octants)
{
    // the params CoverPool gives its own pass pool
    Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    check_cover_octants(pass_bits_of_chunk_and_neighbors(w, pass_pool, COORD, g.div_count()), g, div_size,
                        octants);
}

// Every cell against the model above, then again after a wall goes up and the grid rebuilds.
void test_cover_matches_reference(u32 div_size, ArrayView<const u32> octants)
{
    auto w = world();
    make_cover_scene(w);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    Grid::Cover::Grid g = pool[w[COORD]];
    g.build_if_stale();
    check_cover_octants(w, g, div_size, octants);
    const far_grid far{w, pool, COORD, 3};

    auto& c = w[COORD];
    for (u8 i = 1; i < 15; i++)
        c[{i, 12}].wall_north() = wall_proto();
    rebuild_passability(c);
    far.check("walls");
    g.build_if_stale();
    fm_assert(g.built_octants() == 1u);
    check_cover_octants(w, g, div_size, octants);
}

// Compares octant k with the model when the only blocker lies in the farthest row past the south edge that can
// lower a distance in the chunk. k must point between east and south.
void test_cover_reads_farthest_neighbor_row(u32 div_size, u32 k)
{
    const auto s = cover_step(k);
    const int q = s.x(), p = s.y(), major = Math::max(q, p), dc = (int)(chunk_size_xy / div_size);
    fm_assert(q > 0 && p > 0 && q != p);
    const double len = Math::sqrt((double)s.dot());
    int max_major_offset_with_distance_below_dc = 0;
    while (cover_units((float)((max_major_offset_with_distance_below_dc + 1) * len * div_size / major),
                       div_size) < (u32)dc)
        max_major_offset_with_distance_below_dc++;

    const auto crossed = cover_crossed_cells(k);
    Vector2i blocked_cell_offset;
    for (int t = 0; t*major <= max_major_offset_with_distance_below_dc; t++)
        for (int c = -1; c < (int)crossed.size(); c++)
        {
            const auto offset = t*s + (c < 0 ? Vector2i{} : crossed[(u32)c]);
            const auto major_offset = q > p ? offset.x() : offset.y();
            if (major_offset <= max_major_offset_with_distance_below_dc && offset.y() > blocked_cell_offset.y())
                blocked_cell_offset = offset;
        }
    const auto blocked_cell = Vector2i{0, dc - 1} + blocked_cell_offset;

    constexpr chunk_coords_ south{0, 1, 0};
    auto w = world();
    add_ground_all(w[COORD]);
    add_ground_all(w[south]);
    // A 1-px blocker blocks the cell before its own, and no cell nearer the chunk.
    const auto pixel = (blocked_cell + Vector2i{1, 1 - dc}) * (int)div_size;
    fm_assert(pixel.x() < (int)chunk_size_xy && pixel.y() < (int)chunk_size_xy);
    const auto tile = pixel / (int)tile_size_xy;
    add_blocker(w, {south, local_coords{(u8)tile.x(), (u8)tile.y()}},
                Vector2b{pixel - tile*(int)tile_size_xy - half_tile<Vector2i>}, {1, 1});
    rebuild_passability(w[COORD]);
    rebuild_passability(w[south]);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    const auto passable = pass_bits_of_chunk_and_neighbors(w, cover_pass_pool, COORD, (u32)dc);
    const auto is_passable = [&](int x, int y) { return passable[(usize)((y + dc)*3*dc + x + dc)]; };
    fm_assert(!is_passable(blocked_cell.x(), blocked_cell.y()));
    for (int y = dc; y < blocked_cell.y(); y++)
        for (int x = 0; x < dc; x++)
            fm_assert(is_passable(x, y));

    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    Grid::Cover::Grid g = pool[w[COORD]];
    g.build_if_stale();
    const u32 octants[] = { k };
    check_cover_octants(passable, g, div_size, octants);
    fm_assert(g.distance(Grid::Cover::Grid::get_cell_index(0, (u32)dc - 1, (u32)dc), k) < dc);
}

// make_cover_scene's own chunk, without its neighbors, and with walls a tile short of the chunk's edges.
// Nothing lies within 20 px of the edges.
void make_cover_chunk(world& w, chunk_coords_ ch)
{
    auto& c = w[ch];
    add_ground_all(c);
    for (u8 j = 0; j < TILE_MAX_DIM; j++)
        for (u8 i = 0; i < TILE_MAX_DIM; i++)
        {
            const auto inner = i > 0 && j > 0 && i < TILE_MAX_DIM - 1 && j < TILE_MAX_DIM - 1;
            if (inner && j == 5 && i != 7 && i != 8)
                c[{i, j}].wall_north() = wall_proto();
            if (inner && i == 10 && j != 3 && j != 11)
                c[{i, j}].wall_west() = wall_proto();
            if ((i*7 + j*3) % 5 == 0)
            {
                scenery_proto p;
                p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
                p.subtype   = generic_scenery_proto{};
                p.bbox_size = {24, 24};
                p.pass      = pass_mode::blocked;
                w.make_scenery(w.make_id(), {ch, local_coords{i, j}}, move(p));
            }
        }
    rebuild_passability(c);
}

// A chunk at the world's limit holds the distances the same chunk holds alone at COORD, although the chunks that
// its neighbors' coordinates wrap to are full of blockers. Each chunk gets its own world.
void test_cover_world_edge(u32 div_size)
{
    const auto built = [div_size](chunk_coords_ ch, auto&& fn) {
        auto w = world();
        make_cover_chunk(w, ch);
        for (i32 dy = -1; dy <= 1; dy++)
            for (i32 dx = -1; dx <= 1; dx++)
            {
                const i32 x = ch.x + dx, y = ch.y + dy;
                if (x >= chunk_xy_min && x <= chunk_xy_max && y >= chunk_xy_min && y <= chunk_xy_max)
                    continue;
                const chunk_coords_ wrapped{(i16)x, (i16)y, ch.z};
                auto& c = w[wrapped];
                add_ground_all(c);
                // The blockers of a border tile reach 16 px across the chunk's border.
                for (u8 j = 0; j < TILE_MAX_DIM; j++)
                    for (u8 i = 0; i < TILE_MAX_DIM; i++)
                        add_blocker(w, {wrapped, local_coords{i, j}}, {}, {96, 96});
                rebuild_passability(c);
            }
        Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
        Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, pass_pool};
        Grid::Cover::Grid g = pool[w[ch]];
        g.build_if_stale();
        for (auto k = 0u; k < Grid::Cover::octant_count; k++)
            (void)g.ensure_octant(k);
        fm_assert(g.built_octants() == (u32)-1);
        fn(g);
    };
    const auto dc = chunk_size_xy / div_size;
    Array<u16> expected{NoInit, (usize)(dc*dc*Grid::Cover::octant_count)};
    built(COORD, [&](Grid::Cover::Grid& g) {
        for (auto k = 0u; k < Grid::Cover::octant_count; k++)
            for (auto i = 0u; i < dc*dc; i++)
                expected[k*dc*dc + i] = g.distance(i, k);
    });

    constexpr auto lo = (i16)chunk_xy_min, hi = (i16)chunk_xy_max;
    for (const auto xy : { Vector2s{hi, hi}, Vector2s{hi, lo}, Vector2s{lo, hi}, Vector2s{lo, lo},
                           Vector2s{hi, 0}, Vector2s{lo, 0}, Vector2s{0, hi}, Vector2s{0, lo} })
        built(chunk_coords_{xy.x(), xy.y(), 0}, [&](Grid::Cover::Grid& g) {
            for (auto k = 0u; k < Grid::Cover::octant_count; k++)
                for (auto i = 0u; i < dc*dc; i++)
                    if (g.distance(i, k) != expected[k*dc*dc + i])
                    {
                        Error{standard_error()} << "!!! fatal: div_size" << div_size << "chunk" << xy
                                                << "octant" << k << "cell" << i % dc << i / dc << "distance"
                                                << g.distance(i, k) << "but expected" << expected[k*dc*dc + i];
                        fm_assert(false);
                    }
        });
}

// A chunk without neighbors holds the distances it holds among eight neighbors of bare ground. Four side neighbors
// whose only blocked cells are a whole chunk away change nothing either, with the corner chunks missing.
void test_cover_missing_neighbors_read_as_open_ground(u32 div_size)
{
    const auto dc = chunk_size_xy / div_size;
    const auto distances_with = [&](void (*add_neighbors)(world&)) {
        auto w = world();
        make_cover_chunk(w, COORD);
        add_neighbors(w);
        Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
        Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, pass_pool};
        Grid::Cover::Grid g = pool[w[COORD]];
        g.build_if_stale();
        Array<u16> ret{NoInit, (usize)(dc*dc*Grid::Cover::octant_count)};
        for (auto k = 0u; k < Grid::Cover::octant_count; k++)
        {
            (void)g.ensure_octant(k);
            for (auto i = 0u; i < dc*dc; i++)
                ret[k*dc*dc + i] = g.distance(i, k);
        }
        return ret;
    };
    const auto expected = distances_with([](world&) {});
    const auto expect_same_distances = [&](const char* neighbors, const Array<u16>& distances) {
        for (auto k = 0u; k < Grid::Cover::octant_count; k++)
            for (auto i = 0u; i < dc*dc; i++)
                if (distances[k*dc*dc + i] != expected[k*dc*dc + i])
                {
                    Error{standard_error()} << "!!! fatal: div_size" << div_size << "with" << neighbors
                                            << "octant" << k << "cell" << i % dc << i / dc << "distance"
                                            << distances[k*dc*dc + i] << "but expected" << expected[k*dc*dc + i];
                    fm_assert(false);
                }
    };

    expect_same_distances("ground on all eight neighbors", distances_with([](world& w) {
        for (i16 y = -1; y <= 1; y++)
            for (i16 x = -1; x <= 1; x++)
                if (x != 0 || y != 0)
                {
                    auto& c = w[chunk_coords_{x, y, 0}];
                    add_ground_all(c);
                    rebuild_passability(c);
                }
    }));

    // Each blocker lies just past its chunk's far edge, so it blocks only the chunk's last row or column of cells.
    expect_same_distances("a far blocker in each side neighbor", distances_with([](world& w) {
        const struct { chunk_coords_ ch; local_coords tile; Vector2b bbox_offset; } far_blockers[] = {
            { {1, 0, 0}, {15, 8}, {40, 0} }, { {-1, 0, 0}, {0, 8}, {-40, 0} },
            { {0, 1, 0}, {8, 15}, {0, 40} }, { {0, -1, 0}, {8, 0}, {0, -40} },
        };
        for (const auto& [ch, tile, bbox_offset] : far_blockers)
        {
            auto& c = w[ch];
            add_ground_all(c);
            add_blocker(w, {ch, tile}, bbox_offset, {16, 16});
            rebuild_passability(c);
        }
    }));
}

void test_cover_octant_directions()
{
    for (auto k = 0u; k < Grid::Cover::octant_count; k++)
    {
        const auto dir = Grid::Cover::octant_direction(k);
        fm_assert((dir - cover_direction(k)).length() < 1e-6f);
        const auto theta = Rad{2.f * Math::Constants<float>::pi() * (float)k / (float)Grid::Cover::octant_count};
        const auto c = Math::cos(theta), s = Math::sin(theta);
        const auto off = Deg{Rad{std::atan2(dir.y()*c - dir.x()*s, dir.x()*c + dir.y()*s)}};
        fm_assert(Math::abs(float(off)) <= .7f);
    }
}

// GCC 16.2 miscompiled expand_pass_row() with -fprofile-generate -funroll-loops. With 8 u16 lanes, LoadMaskBits()
// zeroes a register and loads one byte into its low 8 bits. In the unrolled loop's first copy x / 8 is 0, and GCC
// used one register as both that zero index and the mask: xor eax,eax, then mov al,[rcx+rax]. An i386 peephole,
// which checks only that both instructions name the same register, made that movzx eax,[rcx+rax] and dropped the
// xor, so the address used rax while it still held __memcpy_chk's return value, &bits. rcx held &bits too, so a
// later pass wrote movzx r8d,[rax+rax], a read at twice the stack address. The later unrolled copies are correct.
void test_expand_pass_row()
{
    // expand_pass_row() takes whole vectors, which have up to 16 lanes.
    for (auto dc = 16u; dc <= chunk_size_xy; dc += 16)
    {
        for (auto pattern = 0u; pattern < 4; pattern++)
        {
            auto bits = Array<usize>{NoInit, (3*dc + 64 + sizeof(usize)*8 - 1) / (sizeof(usize)*8)};
            for (auto i = 0u; i < bits.size(); i++)
            {
                const auto b = (usize)((i + 1) * 0x9e3779b97f4a7c15u);
                const usize words[] = { 0, ~usize{0}, b, ~b };
                bits[i] = words[pattern];
            }
            for (auto y = 0u; y < 3; y++)
            {
                auto out = Array<u16>{DirectInit, dc, u16{0x1234}};
                Grid::detail::expand_pass_row(out.data(), bits.data(), y*dc, dc);
                for (auto x = 0u; x < dc; x++)
                {
                    const auto expected = (u16)(pass_bit(bits.data(), y*dc + x) ? 0xffff : 0);
                    if (out[x] != expected)
                    {
                        ERR << "!!! fatal: dc" << dc << "pattern" << pattern << "row" << y << "cell" << x
                            << "mask" << out[x] << "but expected" << expected;
                        fm_abort("expand_pass_row() gave a wrong mask");
                    }
                }
            }
        }
    }
}

// A distance is at most a unit above the ray cast from the cell's center along the octant, since the pass grid
// blocks every cell a collider touches. Three blockers lie across COORD's borders.
void test_cover_distance_never_exceeds_ray(u32 div_size, ArrayView<const u32> octants)
{
    auto w = world();
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
        {
            auto& ch = w[chunk_coords_{x, y, 0}];
            add_ground_all(ch);
            // a tile off the chunk's edges, so none reaches into COORD
            if (x != 0 || y != 0)
                for (u8 i = 1; i + 1u < TILE_MAX_DIM; i++)
                {
                    ch[{i, 8}].wall_north() = wall_proto();
                    ch[{8, i}].wall_west() = wall_proto();
                }
        }
    add_blocker(w, {COORD, local_coords{15, 5}}, {29, 0}, {16, 40});
    add_blocker(w, {COORD, local_coords{6, 15}}, {0, 27}, {48, 14});
    add_blocker(w, {chunk_coords_{1, 1, 0}, local_coords{0, 0}}, {-29, -29}, {10, 10});
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
            rebuild_passability(w[chunk_coords_{x, y, 0}]);

    auto& c = w[COORD];
    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    Grid::Cover::Grid g = pool[c];
    g.build_if_stale();

    Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    tick(w, pass_pool);
    const auto dc = g.div_count();
    const auto every = Math::max(1u, dc*dc / 1024);
    for (auto k : octants)
    {
        (void)g.ensure_octant(k);
        u32 hits = 0, cells = 0;
        for (auto y = 0u; y < dc; y++)
            for (auto x = 0u; x < dc; x++)
            {
                if ((x*7 + y*13) % every != 0)
                    continue;
                const auto d = (u32)g.distance(Grid::Cover::Grid::get_cell_index(x, y, dc), k);
                if (d == Grid::Cover::blocked)
                    continue;
                const auto ray = cover_units(cover_ray_px(c, x, y, div_size, cover_ray_delta(k), pass_pool), div_size);
                if (d > ray + 1)
                {
                    Error{standard_error()} << "!!! fatal: div_size" << div_size << "octant" << k << "cell" << x << y
                                            << "distance" << d << "but the ray" << ray;
                    fm_assert(false);
                }
                hits += ray < dc;
                cells++;
            }
        fm_assert(hits >= cells / 4);
    }
}

// The same obstacles at four places, with chunk borders running between them at three. A cell's distances depend
// on where it lies among the obstacles, except for one unit of rounding once its row is numbered differently.
void test_cover_ignores_chunk_borders(u32 div_size, ArrayView<const u32> octants)
{
    // The compared cells are those of tiles 4 to 11 from the obstacles' origin tile, on both axes.
    constexpr u32 first_tile = 4, num_tiles = 8;
    const auto dc = chunk_size_xy / div_size, cells_per_tile = tile_size<u32> / div_size;
    const auto cells_per_side = num_tiles * cells_per_tile;

    const auto distances_at = [&](Vector2i origin) {
        auto w = world();
        for (i16 y = -1; y <= 2; y++)
            for (i16 x = -1; x <= 2; x++)
                add_ground_all(w[chunk_coords_{x, y, 0}]);
        const auto tile_at = [&](int x, int y) {
            return global_coords{COORD, local_coords{}} + origin + Vector2i{x, y};
        };
        add_blocker(w, tile_at(9, 5), {}, {64, 64});
        add_blocker(w, tile_at(3, 9), {5, -7}, {24, 16});
        add_blocker(w, tile_at(11, 10), {-11, 3}, {16, 40});
        for (auto i = 4; i <= 7; i++)
        {
            const auto pos = tile_at(i, 3);
            w[pos.chunk3()][pos.local()].wall_north() = wall_proto();
        }
        for (auto j = 6; j <= 8; j++)
        {
            const auto pos = tile_at(13, j);
            w[pos.chunk3()][pos.local()].wall_west() = wall_proto();
        }
        for (i16 y = -1; y <= 2; y++)
            for (i16 x = -1; x <= 2; x++)
                rebuild_passability(w[chunk_coords_{x, y, 0}]);

        Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
        Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, pass_pool};
        Grid::Cover::Grid grids[] = {
            pool[w[chunk_coords_{0, 0, 0}]], pool[w[chunk_coords_{1, 0, 0}]],
            pool[w[chunk_coords_{0, 1, 0}]], pool[w[chunk_coords_{1, 1, 0}]],
        };
        for (auto& g : grids)
            g.build_if_stale();
        const auto first_x = ((u32)origin.x() + first_tile) * cells_per_tile;
        const auto first_y = ((u32)origin.y() + first_tile) * cells_per_tile;
        Array<u16> ret{NoInit, (usize)(octants.size()*cells_per_side*cells_per_side)};
        for (auto o = 0u; o < octants.size(); o++)
        {
            for (auto& g : grids)
                (void)g.ensure_octant(octants[o]);
            for (auto j = 0u; j < cells_per_side; j++)
                for (auto i = 0u; i < cells_per_side; i++)
                {
                    const auto x = first_x + i, y = first_y + j;
                    const auto& g = grids[y / dc * 2 + x / dc];
                    const auto cell = Grid::Cover::Grid::get_cell_index(x % dc, y % dc, dc);
                    ret[(o*cells_per_side + j)*cells_per_side + i] = g.distance(cell, octants[o]);
                }
        }
        return ret;
    };

    const auto expected = distances_at({0, 0});
    u32 hits = 0;
    for (auto d : expected)
        hits += d < dc;
    fm_assert(hits >= expected.size() / 8);

    for (const auto origin : { Vector2i{8, 0}, Vector2i{0, 8}, Vector2i{8, 8} })
    {
        const auto distances = distances_at(origin);
        for (auto o = 0u; o < octants.size(); o++)
        {
            // Moving the obstacles along y renumbers the rows, and a row's rounded step depends on its number.
            const u32 max_difference = origin.y() == 0 || octants[o] % 8 == 0 ? 0 : 1;
            for (auto i = 0u; i < cells_per_side*cells_per_side; i++)
            {
                const u32 d = distances[o*cells_per_side*cells_per_side + i];
                const u32 e = expected[o*cells_per_side*cells_per_side + i];
                if (Math::max(d, e) - Math::min(d, e) > max_difference)
                {
                    Error{standard_error()} << "!!! fatal: div_size" << div_size << "origin tile" << origin
                                            << "octant" << octants[o] << "cell" << i % cells_per_side
                                            << i / cells_per_side << "distance" << d << "but expected" << e;
                    fm_assert(false);
                }
            }
        }
    }
}

// Octant 0 counts the cells up to the first blocked one, which here lies in the east neighbor.
void test_cover_east_distance_across_border(u32 div_size)
{
    auto w = world();
    for (auto ch : { COORD, COORD_E })
        add_ground_all(w[ch]);
    constexpr struct { u8 j; i8 bbox_offset_x; } blockers[] = {
        { 2, -13 }, { 5, -12 }, { 8, -11 }, { 11, -3 },
    };
    constexpr auto bbox_size = Vector2ub{16, 32};
    for (auto [j, bx] : blockers)
        add_blocker(w, {COORD_E, local_coords{0, j}}, {bx, 0}, bbox_size);
    for (auto ch : { COORD, COORD_E })
        rebuild_passability(w[ch]);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    Grid::Cover::Grid g = pool[w[COORD]];
    g.build_if_stale();

    const auto dc = g.div_count();
    const auto box = (int)Math::max(div_size, Pass::Grid::min_bbox_size);
    const auto center = [div_size](u32 i) { return (int)(i*div_size + div_size/2) - half_tile<int>; };
    for (auto [j, bx] : blockers)
    {
        const auto face = (int)chunk_size_xy + bx - bbox_size.x()/2;
        // The pass grid blocks the cells holding a position within box/2 - 1 px of the blocker.
        const auto first_blocked = (u32)(face + half_tile<int> - box/2 + 1) / div_size;
        fm_assert(first_blocked >= dc);
        const auto y0 = tile_size_xy*j - bbox_size.y()/2, y1 = y0 + bbox_size.y();
        u32 rows = 0;
        for (auto y = 0u; y < dc; y++)
        {
            // only rows through the blocker's own pixels
            if (center(y) <= y0 || center(y) >= y1)
                continue;
            rows++;
            for (auto x = 0u; x < dc; x++)
            {
                const auto expected = Math::min(dc, first_blocked - x);
                const auto d = (u32)g.distance(Grid::Cover::Grid::get_cell_index(x, y, dc), 0);
                if (d != expected)
                {
                    Error{standard_error()} << "!!! fatal: div_size" << div_size << "cell" << x << y
                                            << "distance" << d << "but expected" << expected;
                    fm_assert(false);
                }
            }
        }
        fm_assert(rows > 0);
    }
}

void test_cover_blocked_cells(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_blocker(w, {COORD, local_coords{5, 5}}, {3, -5}, {40, 24});
    rebuild_passability(c);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    Grid::Cover::Grid g = pool[c];
    g.build_if_stale();

    Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    tick(w, pass_pool);
    Pass::Grid pg = pass_pool[c];
    pg.build_if_stale();

    const auto dc = g.div_count();
    for (auto k : { 0u, 4u, 5u })
    {
        (void)g.ensure_octant(k);
        u32 blocked = 0;
        for (auto y = 0u; y < dc; y++)
            for (auto x = 0u; x < dc; x++)
            {
                const auto d = g.distance(Grid::Cover::Grid::get_cell_index(x, y, dc), k);
                const bool passable = pg.bit(Pass::Grid::get_bitmask_index(x, y, dc));
                fm_assert(passable == (d != Grid::Cover::blocked));
                fm_assert(!passable || (d >= 1 && d <= dc));
                blocked += !passable;
            }
        fm_assert(blocked > 0);
    }
}

// Each fill_non_octilinear_octant<T>() has its own inlined copy of expand_pass_row(), which test_expand_pass_row()
// doesn't run. The blockers make the first byte of a row's pass bits differ between rows.
void test_cover_non_octilinear_octant_blocked_cells(u32 div_size, ArrayView<const u32> octants)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        add_blocker(w, {COORD, local_coords{0, (u8)j}}, {(i8)((j*7) % 29 - 20), (i8)((j*11) % 23 - 12)},
                    {(u8)(3 + (j*5) % 13), (u8)(5 + (j*3) % 13)});
    rebuild_passability(c);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    Grid::Cover::Grid g = pool[c];
    g.build_if_stale();

    Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    tick(w, pass_pool);
    Pass::Grid pg = pass_pool[c];
    pg.build_if_stale();

    const auto dc = g.div_count();
    u32 mixed_rows = 0;
    for (auto y = 0u; y < dc; y++)
    {
        u32 passable = 0;
        for (auto x = 0u; x < 8; x++)
            passable += pg.bit(Pass::Grid::get_bitmask_index(x, y, dc));
        mixed_rows += passable > 0 && passable < 8;
    }
    if (mixed_rows < TILE_MAX_DIM)
    {
        ERR << "!!! fatal: div_size" << div_size << "only" << mixed_rows
            << "rows have both kinds of cells among their first 8";
        fm_abort("the blockers don't vary the first byte of the rows");
    }

    for (auto k : octants)
    {
        (void)g.ensure_octant(k);
        for (auto y = 0u; y < dc; y++)
            for (auto x = 0u; x < dc; x++)
            {
                const auto d = g.distance(Grid::Cover::Grid::get_cell_index(x, y, dc), k);
                const bool passable = pg.bit(Pass::Grid::get_bitmask_index(x, y, dc));
                if (passable != (d != Grid::Cover::blocked) || passable && d > dc)
                {
                    ERR << "!!! fatal: div_size" << div_size << "octant" << k << "cell" << x << y
                        << "distance" << d << "but passable" << passable;
                    fm_abort("a non-octilinear octant's cell disagrees with the pass grid");
                }
            }
    }
}

// Each neighbor change below marks the grid stale through a different path: chunk creation,
// mark_passability_modified, chunk deletion.
void test_cover_rebuilds_on_neighbor_change(u32 div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    const auto build = [&] {
        Grid::Cover::Grid g = pool[c];
        g.build_if_stale();
        return g.build_no();
    };
    auto n = build();
    fm_assert(build() == n);
    const far_grid far{w, pool, COORD_E, 3};

    auto& e = w[COORD_E];
    far.check("creating");
    fm_assert(build() != n);
    n = build();
    fm_assert(build() == n);

    add_ground_all(e);
    rebuild_passability(e);
    far.check("adding ground");
    fm_assert(build() != n);

    clear_ground_all(e);
    n = build();
    w.collect(true, true);
    fm_assert(w.at(COORD_E) == nullptr);
    far.check("collecting");
    fm_assert(build() != n);
}

// A blocker of the chunk two east overhangs the east neighbor's far edge, which octant 0 reaches from the last
// column. Adding it must rebuild the grid, although the blocker's chunk isn't a neighbor.
void test_cover_rebuilds_on_ring2_change(u32 div_size)
{
    constexpr auto E2 = chunk_coords_{2, 0, 0};
    auto w = world();
    for (auto ch : { COORD, COORD_E, E2 })
        add_ground_all(w[ch]);
    for (auto ch : { COORD, COORD_E, E2 })
        rebuild_passability(w[ch]);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    const auto dc = chunk_size_xy / div_size;
    // a row through tile row 8, which the blocker spans
    const auto cell = Grid::Cover::Grid::get_cell_index(dc - 1, (u32)(tile_size_xy*8 + half_tile<int>) / div_size, dc);
    const auto east = [&](Grid::Cover::Pool& p) {
        Grid::Cover::Grid g = p[w[COORD]];
        g.build_if_stale();
        (void)g.ensure_octant(0);
        return (u32)g.distance(cell, 0);
    };
    fm_assert(east(pool) == dc);
    const far_grid far{w, pool, E2, 3};

    // x from 2048 - 80 to 2048 - 48 in COORD's frame
    add_blocker(w, {E2, local_coords{0, 8}}, {-64, 0}, {32, 64});
    rebuild_passability(w[E2]);
    far.check("a blocker");
    const auto face = 2*(int)chunk_size_xy - 80, box = (int)Math::max(div_size, Pass::Grid::min_bbox_size);
    // cells from the last column to the first one the east neighbor's pass grid blocks
    const auto expected = (u32)(face + half_tile<int> - box/2 + 1) / div_size - (dc - 1);
    fm_assert(expected < dc);
    {
        Pass::Pool fresh_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
        Grid::Cover::Pool fresh{Grid::Cover::Params{div_size}, fresh_pass_pool};
        fm_assert(east(fresh) == expected);
    }
    const auto d = east(pool);
    if (d != expected)
    {
        Error{standard_error()} << "!!! fatal: div_size" << div_size << "distance" << d << "but expected" << expected;
        fm_assert(false);
    }
}

// A wall of the chunk two east lies in the east neighbor's area. The grid must rebuild once collect() deletes the
// wall's chunk.
void test_cover_rebuilds_on_ring2_collect(u32 div_size)
{
    constexpr auto E2 = chunk_coords_{2, 0, 0};
    auto w = world();
    for (auto ch : { COORD, COORD_E, E2 })
        add_ground_all(w[ch]);
    // "test1" walls are 32 deep, so this one spans x from 2048 - 64 to 2048 - 32 in COORD's frame.
    w[E2][{0, 8}].wall_west() = wall_image_proto{ loader.wall_atlas("test1"), 0 };
    for (auto ch : { COORD, COORD_E, E2 })
        rebuild_passability(w[ch]);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    const auto dc = chunk_size_xy / div_size;
    const auto cell = Grid::Cover::Grid::get_cell_index(dc - 1, (u32)(tile_size_xy*8 + half_tile<int>) / div_size, dc);
    const auto east = [&] {
        Grid::Cover::Grid g = pool[w[COORD]];
        g.build_if_stale();
        (void)g.ensure_octant(0);
        return (u32)g.distance(cell, 0);
    };
    const auto face = 2*(int)chunk_size_xy - 64, box = (int)Math::max(div_size, Pass::Grid::min_bbox_size);
    const auto expected = (u32)(face + half_tile<int> - box/2 + 1) / div_size - (dc - 1);
    fm_assert(expected < dc);
    fm_assert_equal(expected, east());
    const far_grid far{w, pool, E2, 3};

    // The tile writes mark nothing, so only collect() can make the grid rebuild.
    w[E2][{0, 8}].wall_west() = wall_image_proto{};
    clear_ground_all(w[E2]);
    w.collect(true, true);
    fm_assert(!w.at(E2));
    far.check("collecting");
    fm_assert_equal(dc, east());
}

// A blocker of the chunk two east overhangs the east neighbor's area. Octant 0 reaches it only while that neighbor
// exists, since a missing chunk has no pass grid.
void test_cover_overhang_into_missing_chunk_reads_open(u32 div_size)
{
    constexpr auto E2 = chunk_coords_{2, 0, 0};
    auto w = world();
    for (auto ch : { COORD, E2 })
        add_ground_all(w[ch]);
    // x from 2048 - 80 to 2048 - 48 in COORD's frame
    add_blocker(w, {E2, local_coords{0, 8}}, {-64, 0}, {32, 64});
    for (auto ch : { COORD, E2 })
        rebuild_passability(w[ch]);

    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    const auto dc = chunk_size_xy / div_size;
    const auto cell = Grid::Cover::Grid::get_cell_index(dc - 1, (u32)(tile_size_xy*8 + half_tile<int>) / div_size, dc);
    const auto east = [&] {
        Grid::Cover::Grid g = pool[w[COORD]];
        g.build_if_stale();
        (void)g.ensure_octant(0);
        return (u32)g.distance(cell, 0);
    };
    fm_assert_equal(dc, east());

    add_ground_all(w[COORD_E]);
    rebuild_passability(w[COORD_E]);
    const auto face = 2*(int)chunk_size_xy - 80, box = (int)Math::max(div_size, Pass::Grid::min_bbox_size);
    const auto expected = (u32)(face + half_tile<int> - box/2 + 1) / div_size - (dc - 1);
    fm_assert(expected < dc);
    fm_assert_equal(expected, east());

    clear_ground_all(w[COORD_E]);
    w.collect(true, true);
    fm_assert(!w.at(COORD_E));
    fm_assert_equal(dc, east());
}

enum class expect : u8 { current, stale, either };

expect expect_stale(bool stale)
{
    return stale ? expect::stale : expect::current;
}

template<typename G>
void check_stale(G& g, u64& saved, expect e, const char* grid, const char* what, chunk_coords_ ch)
{
    const bool stale = g.is_marked_stale();
    fm_assert(g.is_stale() == stale);
    fm_assert(g.is_stale(saved) == stale);
    if (e != expect::either && stale != (e == expect::stale))
    {
        ERR << "!!! fatal:" << grid << "grid" << (stale ? "stale" : "current") << "after" << what
            << "chunk" << (int)ch.x << (int)ch.y << (int)ch.z;
        fm_abort("is_marked_stale() is wrong");
    }
    g.build_if_stale();
    fm_assert(!g.is_stale());
    fm_assert(g.is_stale(saved) == stale);
    saved = g.built_pass_gen();
}

struct stale_grids
{
    world& w;
    Pass::Pool pass_pool{Pass::Params{16, pass_mode::pass}};
    Grid::Object::Pool object_pool{pass_pool};
    Pass::Pool cover_pass_pool{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool cover_pool{Grid::Cover::Params{16}, cover_pass_pool};
    u64 pass_saved = 0, object_saved = 0, cover_saved = 0;

    explicit stale_grids(world& w): w{w}
    {
        auto pg = pass_pool[w[COORD]];
        auto og = object_pool[w[COORD]];
        auto cg = cover_pool[w[COORD]];
        pg.build_if_stale();
        og.build_if_stale();
        cg.build_if_stale();
        pass_saved = pg.built_pass_gen();
        object_saved = og.built_pass_gen();
        cover_saved = cg.built_pass_gen();
    }

    void check(chunk_coords_ ch, const char* what, auto&& change, expect pass, expect cover)
    {
        change();
        auto pg = pass_pool[w[COORD]];
        auto og = object_pool[w[COORD]];
        auto cg = cover_pool[w[COORD]];
        check_stale(pg, pass_saved, pass, "pass", what, ch);
        check_stale(og, object_saved, pass, "object", what, ch);
        check_stale(cg, cover_saved, cover, "cover", what, ch);

        Pass::Pool fresh{pass_pool.params()};
        auto f = fresh[w[COORD]];
        f.build_if_stale();
        for (auto k = 0u; k < pg.div_count() * pg.div_count(); k++)
            if (pg.bit(k) != f.bit(k))
            {
                ERR << "!!! fatal: pass grid differs from a fresh build after" << what
                    << "chunk" << (int)ch.x << (int)ch.y << (int)ch.z;
                fm_abort("pass grid differs from a fresh build");
            }
    }
};

void test_is_stale_on_chunk_change()
{
    constexpr auto above = chunk_coords_{0, 0, 1};
    auto w = world();
    for (auto y = -3; y <= 3; y++)
        for (auto x = -3; x <= 3; x++)
            add_ground_all(w[chunk_coords_{(i16)x, (i16)y, 0}]);
    add_ground_all(w[above]);
    for (auto& c : w.chunks())
        rebuild_passability(c);

    stale_grids s{w};
    for (auto y = -3; y <= 3; y++)
        for (auto x = -3; x <= 3; x++)
        {
            const auto ch = chunk_coords_{(i16)x, (i16)y, 0};
            const auto d = Math::max(Math::abs(x), Math::abs(y));
            s.check(ch, "rebuild_passability", [&] { rebuild_passability(w[ch]); },
                    expect_stale(d <= 1), expect_stale(d <= 2));
        }
    s.check(above, "rebuild_passability", [&] { rebuild_passability(w[above]); },
            expect::current, expect::current);
}

void test_is_stale_on_chunk_create_and_collect()
{
    auto w = world();
    add_ground_all(w[COORD]);
    rebuild_passability(w[COORD]);

    stale_grids s{w};
    const auto check = [&](chunk_coords_ ch, expect pass, expect cover, bool restamps) {
        s.check(ch, "creating", [&] { (void)w[ch]; }, pass, cover);
        const auto gen = w.max_pass_gen(COORD, 0);
        s.check(ch, "collecting", [&] { w.collect(true, true); fm_assert(!w.at(ch)); }, pass, cover);
        fm_assert((w.max_pass_gen(COORD, 0) > gen) == restamps);
    };
    for (auto y = -3; y <= 3; y++)
        for (auto x = -3; x <= 3; x++)
            if (x || y)
            {
                const auto d = Math::max(Math::abs(x), Math::abs(y));
                check({(i16)x, (i16)y, 0}, expect_stale(d <= 1), expect_stale(d <= 2), d <= 2);
            }
    check({0, 0, 1}, expect::current, expect::current, false);
}

// A grid marked stale is rebuilt first, which fills octant 0, so ensure_octant(k) fills one octant or two.
void test_cover_octant_fills_include_rebuild()
{
    auto w = world();
    add_ground_all(w[COORD]);
    rebuild_passability(w[COORD]);
    Pass::Pool cover_pass_pool{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool cover_pool{Grid::Cover::Params{16}, cover_pass_pool};
    auto g = cover_pool[w[COORD]];
    const auto num_filled = [&](auto&& fill_octants) {
        const auto fills_at_start = cover_pool.octant_fills();
        const u32 n = fill_octants();
        fm_assert_equal(cover_pool.octant_fills() - fills_at_start, n);
        return n;
    };
    const auto ensure = [&](u32 k) { return num_filled([&] { return g.ensure_octant(k); }); };
    const auto fill_next = [&] { return num_filled([&] { return g.fill_next_unfilled_octant(); }); };

    fm_assert(g.is_marked_stale());
    fm_assert_equal(1u, ensure(0));
    fm_assert_equal(0u, ensure(0));
    fm_assert_equal(1u, ensure(5));
    g.mark_stale();
    fm_assert_equal(2u, ensure(5));
    g.mark_stale();
    fm_assert_equal(1u, ensure(0));
    g.mark_stale();
    fm_assert_equal(2u, fill_next());
    for (auto i = 2u; i < Grid::Cover::octant_count; i++)
        fm_assert_equal(1u, fill_next());
    fm_assert_equal(0u, fill_next());
    fm_assert_equal((u32)-1, g.built_octants());
}

void test_is_stale_lifecycle()
{
    constexpr chunk_coords_ N{1, -1, 0}, R{-2, 1, 0}, F{5, 5, 0};
    auto w = world();
    for (auto ch : { COORD, N, R, F })
        add_ground_all(w[ch]);
    for (auto ch : { COORD, N, R, F })
        rebuild_passability(w[ch]);

    Pass::Pool pass_pool{Pass::Params{16, pass_mode::pass}};
    Pass::Pool cover_pass_pool{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool cover_pool{Grid::Cover::Params{16}, cover_pass_pool};
    auto pg = pass_pool[w[COORD]];
    auto cg = cover_pool[w[COORD]];
    const auto each = [&](auto&& f) { f(pg); f(cg); };
    const auto build = [&] {
        each([](auto& g) {
            g.build_if_stale();
            fm_assert(!g.is_stale());
        });
        fm_assert(pg.built_pass_gen() == cg.built_pass_gen());
        return pg.built_pass_gen();
    };

    each([](auto& g) {
        fm_assert(g.is_marked_stale());
        fm_assert(g.is_stale());
        fm_assert(g.is_stale(0));
    });
    const auto s0 = build();
    each([&](auto& g) {
        fm_assert(!g.is_stale(s0));
        fm_assert(g.is_stale(0));
    });

    for (auto k : { 8u, 4u, 1u })
    {
        const bool filled = cg.ensure_octant(k);
        fm_assert(filled);
        fm_assert(!cg.is_stale());
        fm_assert(!cg.is_stale(s0));
    }

    each([&](auto& g) {
        g.mark_stale();
        fm_assert(g.is_stale());
        fm_assert(g.is_stale(s0));
    });
    (void)build();
    each([&](auto& g) { fm_assert(!g.is_stale(s0)); });

    rebuild_passability(w[F]);
    each([&](auto& g) {
        fm_assert(!g.is_marked_stale());
        fm_assert(!g.is_stale());
        fm_assert(!g.is_stale(s0));
    });

    rebuild_passability(w[N]);
    each([&](auto& g) {
        fm_assert(g.is_marked_stale());
        fm_assert(g.is_stale());
        fm_assert(g.is_stale(s0));
    });
    rebuild_passability(w[R]);
    rebuild_passability(w[N]);
    each([&](auto& g) {
        fm_assert(g.is_marked_stale());
        fm_assert(g.is_stale());
        fm_assert(g.is_stale(s0));
    });
    const auto s1 = build();
    each([&](auto& g) {
        fm_assert(g.is_stale(s0));
        fm_assert(!g.is_stale(s1));
    });

    rebuild_passability(w[N]);
    clear_ground_all(w[N]);
    w.collect(true, true);
    fm_assert(!w.at(N));
    each([&](auto& g) {
        fm_assert(g.is_marked_stale());
        fm_assert(g.is_stale(s1));
    });
    const auto s2 = build();
    each([&](auto& g) {
        fm_assert(g.is_stale(s1));
        fm_assert(!g.is_stale(s2));
    });

    clear_ground_all(w[COORD]);
    w.collect(true, true);
    fm_assert(!w.at(COORD));
    auto& c = w[COORD];
    const auto pass_free_count = pass_pool.free_grid_count(), cover_free_count = cover_pool.free_grid_count();
    pg = pass_pool[c];
    cg = cover_pool[c];
    fm_assert(pass_pool.free_grid_count() == pass_free_count - 1);
    fm_assert(cover_pool.free_grid_count() == cover_free_count - 1);
    each([&](auto& g) {
        fm_assert(g.is_marked_stale());
        fm_assert(g.is_stale());
        fm_assert(g.is_stale(s2));
    });
    const auto s3 = build();
    each([&](auto& g) {
        fm_assert(g.is_stale(s2));
        fm_assert(!g.is_stale(s3));
    });

    auto w2 = world(move(w));
    each([&](auto& g) {
        fm_assert(!g.is_stale());
        fm_assert(!g.is_stale(s3));
    });
    (void)w2[chunk_coords_{2, -2, 0}];
    fm_assert(!pg.is_stale());
    fm_assert(!pg.is_stale(s3));
    fm_assert(cg.is_stale());
    fm_assert(cg.is_stale(s3));
}

void test_pass_blocker_leaves_ring2_current(u32 div_size, u32 bbox_size)
{
    constexpr auto blocker_size = Vector2ub{255, 255};
    const auto params = Pass::Params{div_size, bbox_size, pass_mode::pass};
    const auto at = [](i32 x, i32 y) { return chunk_coords_{(i16)x, (i16)y, 0}; };
    const auto lo = Vector2i{half_tile<i32> + (i32)(bbox_size / 2)};
    const auto hi = Vector2i{(i32)chunk_size_xy - half_tile<i32> - 1 + (i32)(bbox_size - bbox_size / 2)};
    const auto reaches = [&](const Pair<Vector2i, Vector2i>& r, i32 x, i32 y) {
        const auto origin = Vector2i{x, y} * (i32)chunk_size_xy;
        return (r.first() < origin + hi).all() && (origin - lo < r.second()).all();
    };
    const auto tile = [](i8 d) -> u8 { return d < 0 ? 0 : d > 0 ? TILE_MAX_DIM - 1 : TILE_MAX_DIM / 2; };

    for (const auto d : world::neighbor_offsets)
    {
        auto w = world();
        for (auto y = -2; y <= 2; y++)
            for (auto x = -2; x <= 2; x++)
                add_ground_all(w[at(x, y)]);
        for (auto& c : w.chunks())
            rebuild_passability(c);

        Pass::Pool pool{params};
        u32 before[5][5];
        for (auto y = -2; y <= 2; y++)
            for (auto x = -2; x <= 2; x++)
                before[y+2][x+2] = count_passable_built(pool, w[at(x, y)]);

        const auto local = local_coords{tile(d.x()), tile(d.y())};
        const auto bbox_offset = Vector2b{(i8)(d.x() * 127), (i8)(d.y() * 127)};
        const auto blocker = add_blocker(w, {COORD, local}, bbox_offset, blocker_size);
        fm_assert(blocker->bbox_offset == bbox_offset && blocker->bbox_size == blocker_size);
        const auto rect = scenery_tile(local, blocker->offset, blocker->bbox_offset, blocker->bbox_size);

        Pass::Pool fresh{params};
        for (auto y = -2; y <= 2; y++)
            for (auto x = -2; x <= 2; x++)
            {
                const auto ring = Math::max(Math::abs(x), Math::abs(y));
                const bool toward = x == d.x() && y == d.y();
                if (ring == 2)
                    fm_assert(!reaches(rect, x, y));
                if (ring == 0 || toward)
                    fm_assert(reaches(rect, x, y));

                auto& c = w[at(x, y)];
                auto g = pool[c];
                if (g.is_marked_stale() != (ring <= 1))
                {
                    ERR << "!!! fatal: pass grid of chunk" << x << y << (g.is_marked_stale() ? "marked" : "not marked")
                        << "stale after a blocker toward" << (int)d.x() << (int)d.y();
                    fm_abort("wrong pass grids marked stale");
                }
                g.build_if_stale();
                auto f = fresh[c];
                f.build_if_stale();
                const auto dc = g.div_count();
                for (auto i = 0u; i < dc*dc; i++)
                    if (g.bit(i) != f.bit(i))
                    {
                        ERR << "!!! fatal: pass grid of chunk" << x << y << "differs from a fresh build at bit" << i
                            << "after a blocker toward" << (int)d.x() << (int)d.y();
                        fm_abort("pass grid differs from a fresh build");
                    }
                if (ring == 0 || toward)
                    fm_assert(count_passable(g) < before[y+2][x+2]);
            }

        pool[w[at(d.x(), d.y())]].mark_stale();
        for (auto y = -2; y <= 2; y++)
            for (auto x = -2; x <= 2; x++)
            {
                const bool near = Math::max(Math::abs(x - d.x()), Math::abs(y - d.y())) <= 1;
                fm_assert(pool[w[at(x, y)]].is_marked_stale() == near);
            }
    }
}

void test_hole_change_leaves_far_grids_current()
{
    constexpr auto H = COORD, N = COORD_E;
    constexpr u32 west_octant = Grid::Cover::octant_count / 2;
    const auto at = [](i32 x) { return chunk_coords_{(i16)x, 0, 0}; };
    auto w = world();
    for (auto x = -1; x <= 4; x++)
        add_ground_all(w[at(x)]);
    add_blocked_ground_all(w[N]);
    for (auto& c : w.chunks())
        rebuild_passability(c);

    Pass::Pool small{Pass::Params{16, pass_mode::pass}};
    Pass::Pool large{Pass::Params{16, chunk_size_xy, pass_mode::pass}};
    Grid::Object::Pool small_objects{small}, large_objects{large};
    Pass::Pool cover_pass{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool cover{Grid::Cover::Params{16}, cover_pass};
    Pass::Pool* const pass_pools[] = { &small, &large };
    Grid::Object::Pool* const object_pools[] = { &small_objects, &large_objects };

    const auto check = [&](const char* what, auto&& change) {
        u64 build_no[6][3], pass_gen[6], neighbor_gen[6];
        for (auto x = -1; x <= 4; x++)
        {
            auto& c = w[at(x)];
            for (auto i = 0u; i < 2; i++)
            {
                auto g = (*pass_pools[i])[c];
                g.build_if_stale();
                build_no[x+1][i] = g.build_no();
                (*object_pools[i])[c].build_if_stale();
            }
            auto g = cover[c];
            g.build_if_stale();
            (void)g.ensure_octant(west_octant);
            build_no[x+1][2] = g.build_no();
        }
        for (auto x = -1; x <= 4; x++)
        {
            auto& c = w[at(x)];
            c.ensure_passability();
            pass_gen[x+1] = c.pass_gen();
            neighbor_gen[x+1] = c.neighbor_pass_gen();
        }
        const auto gen = w.max_pass_gen(N, 0);
        change();
        fm_assert(w.max_pass_gen(N, 0) > gen);
        for (auto x = -1; x <= 4; x++)
        {
            const auto& c = w[at(x)];
            const auto d = Math::abs(x);
            if (c.is_passability_modified() != (d <= 1) || (c.pass_gen() != pass_gen[x+1]) != (d == 0) ||
                (c.neighbor_pass_gen() != neighbor_gen[x+1]) != (d == 1))
            {
                ERR << "!!! fatal: chunk" << x << "has the wrong dirty flag or stamps after" << what;
                fm_abort("a hole change touched the wrong chunks");
            }
        }

        Pass::Pool fresh_small{small.params()}, fresh_large{large.params()};
        Pass::Pool fresh_cover_pass{cover_pass.params()};
        Grid::Cover::Pool fresh_cover{Grid::Cover::Params{16}, fresh_cover_pass};
        Pass::Pool* const fresh_pools[] = { &fresh_small, &fresh_large };
        for (auto x = -1; x <= 4; x++)
        {
            auto& c = w[at(x)];
            const auto d = Math::abs(x);
            const auto fail = [&](const char* grid, const char* why) {
                ERR << "!!! fatal:" << grid << "grid of chunk" << x << why << "after" << what;
                fm_abort("wrong grid after a hole change");
            };
            for (auto i = 0u; i < 2; i++)
            {
                auto g = (*pass_pools[i])[c];
                auto og = (*object_pools[i])[c];
                if (g.is_marked_stale() != (d <= 1) || g.is_stale() != (d <= 1))
                    fail("pass", "has the wrong staleness");
                if (og.is_marked_stale() != (d <= 1) || og.is_stale() != (d <= 1))
                    fail("object", "has the wrong staleness");
                g.build_if_stale();
                og.build_if_stale();
                if (d > 1 && g.build_no() != build_no[x+1][i])
                    fail("pass", "was rebuilt");
                auto f = (*fresh_pools[i])[c];
                f.build_if_stale();
                for (auto k = 0u; k < g.div_count() * g.div_count(); k++)
                    if (g.bit(k) != f.bit(k))
                        fail("pass", "differs from a fresh build");
            }
            auto g = cover[c];
            if (g.is_marked_stale() != (d <= 2) || g.is_stale() != (d <= 2))
                fail("cover", "has the wrong staleness");
            g.build_if_stale();
            (void)g.ensure_octant(west_octant);
            if (d > 2 && g.build_no() != build_no[x+1][2])
                fail("cover", "was rebuilt");
            auto f = fresh_cover[c];
            f.build_if_stale();
            (void)f.ensure_octant(west_octant);
            for (auto k = 0u; k < g.div_count() * g.div_count(); k++)
                if (g.distance(k, west_octant) != f.distance(k, west_octant))
                    fail("cover", "differs from a fresh build");
        }
    };

    auto proto = hole_proto{};
    proto.offset = {31, 0};
    proto.bbox_offset = {127, 0};
    proto.bbox_size = {255, 255};
    proto.pass = pass_mode::pass;
    bptr<hole> h;
    check("adding", [&] { h = w.make_object<hole>(w.make_id(), global_coords{H, {15, 8}}, proto); });
    const auto rect = scenery_tile(h->coord.local(), h->offset, h->bbox_offset, h->bbox_size);
    fm_assert_equal(254, rect.second().x() - ((i32)chunk_size_xy - half_tile<i32>));
    fm_assert(count_passable_built(small, w[N]) > 0);
    check("disabling", [&] { h->set_enabled(false); });
    fm_assert_equal(0u, count_passable_built(small, w[N]));
    check("enabling", [&] { h->set_enabled(true); });
    check("resizing", [&] { h->set_bbox(h->offset, h->bbox_offset, {254, 255}, h->pass); });
    check("moving", [&] {
        auto i = h->index();
        h->teleport_to(i, h->coord, {30, 0}, h->r);
    });
    fm_assert(count_passable_built(small, w[N]) > 0);
    check("removing", [&] { w[H].remove_object(*h, h->index()); });
    fm_assert_equal(0u, count_passable_built(small, w[N]));
}

void test_cover_grids_after_static_changes()
{
    const auto at = [](i32 x) { return chunk_coords_{(i16)x, 0, 0}; };
    auto w = world();
    for (auto x = -1; x <= 4; x++)
        add_ground_all(w[at(x)]);
    for (auto& c : w.chunks())
        rebuild_passability(c);

    Pass::Pool cover_pass{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool cover{Grid::Cover::Params{16}, cover_pass};

    const auto check = [&](const char* what, auto&& change) {
        for (auto x = -1; x <= 4; x++)
        {
            auto g = cover[w[at(x)]];
            g.build_if_stale();
            for (auto k = 0u; k < Grid::Cover::octant_count; k++)
                (void)g.ensure_octant(k);
        }
        const auto gen = w.max_pass_gen(COORD, 1);
        change();
        fm_assert(w.max_pass_gen(COORD, 1) > gen);

        Pass::Pool fresh_pass{cover_pass.params()};
        Grid::Cover::Pool fresh{Grid::Cover::Params{16}, fresh_pass};
        u32 unmarked = 0;
        for (auto x = -1; x <= 4; x++)
        {
            auto& c = w[at(x)];
            auto g = cover[c];
            if (g.is_marked_stale())
                continue;
            unmarked++;
            auto f = fresh[c];
            f.build_if_stale();
            for (auto k = 0u; k < Grid::Cover::octant_count; k++)
            {
                (void)f.ensure_octant(k);
                for (auto i = 0u; i < g.div_count() * g.div_count(); i++)
                    if (g.distance(i, k) != f.distance(i, k))
                    {
                        ERR << "!!! fatal: unmarked cover grid of chunk" << x << "differs from a fresh build"
                            << "in octant" << k << "cell" << i << "after" << what;
                        fm_abort("a static change left a cover grid unmarked");
                    }
            }
        }
        fm_assert(unmarked > 0);
    };

    check("a blocked ground tile", [&] {
        w[COORD][{15, 2}].ground() = tile_image_proto{ loader.ground_atlas("texel"), 0 };
        w[COORD].mark_ground_modified();
    });
    check("a north wall", [&] {
        w[COORD][{15, 4}].wall_north() = wall_proto();
        w[COORD].mark_walls_modified();
    });
    check("a west wall", [&] {
        w[COORD][{15, 6}].wall_west() = wall_proto();
        w[COORD].mark_walls_modified();
    });

    auto p = scenery_proto(loader.scenery("chair4"));
    p.bbox_offset = {127, 0};
    p.bbox_size   = {32, 32};
    p.pass        = pass_mode::blocked;
    bptr<scenery> s;
    check("adding a blocker", [&] { s = w.make_scenery(w.make_id(), {COORD, {15, 10}}, scenery_proto{p}); });
    fm_assert(!s->is_dynamic());
    auto i = s->index();
    check("moving the blocker", [&] { s->teleport_to(i, {COORD, {15, 11}}, s->offset, s->r); });
    fm_assert(s->atlas->check_rotation(rotation::E));
    check("turning the blocker", [&] { s->rotate(i, rotation::E); });
    check("making the blocker shoot_through", [&] {
        s->set_bbox(s->offset, s->bbox_offset, s->bbox_size, pass_mode::shoot_through);
    });
    check("making the blocker blocked", [&] {
        s->set_bbox(s->offset, s->bbox_offset, s->bbox_size, pass_mode::blocked);
    });
    check("moving the blocker into chunk 1", [&] { s->teleport_to(i, {COORD_E, {0, 11}}, s->offset, s->r); });
    fm_assert(&s->chunk() == &w[COORD_E]);
    check("removing the blocker", [&] { s->chunk().remove_object(*s, i); });

    p.bbox_offset = {};
    check("adding a blocker under the hole", [&] {
        (void)w.make_scenery(w.make_id(), {COORD_E, {1, 8}}, scenery_proto{p});
    });
    auto proto = hole_proto{};
    proto.offset = {31, 0};
    proto.bbox_offset = {127, 0};
    proto.bbox_size = {255, 255};
    proto.pass = pass_mode::pass;
    bptr<hole> h;
    check("adding a hole", [&] { h = w.make_object<hole>(w.make_id(), global_coords{COORD, {15, 8}}, proto); });
    check("disabling the hole", [&] { h->set_enabled(false); });
    check("removing the hole", [&] { w[COORD].remove_object(*h, h->index()); });
}

// Octant 4 adds one step of div_size*sqrt2 px per row up to the first blocked row, which lies in the south
// neighbors. Steps rounded to whole units before adding would be off by up to one more half unit.
void test_cover_diagonal_sums_steps_across_border(u32 div_size)
{
    auto w = world();
    const auto chunks = { COORD, COORD_E, COORD_Eʹ, chunk_coords_{1, 1, 0} };
    for (auto ch : chunks)
        add_ground_all(w[ch]);
    // faces at y 1083 in COORD's frame, off the cell grid
    for (auto ch : { COORD_Eʹ, chunk_coords_{1, 1, 0} })
        for (u8 i = 0; i < TILE_MAX_DIM; i++)
            add_blocker(w, {ch, local_coords{i, 1}}, {0, 3}, {64, 16});
    for (auto ch : chunks)
        rebuild_passability(w[ch]);

    auto& c = w[COORD];
    Pass::Pool cover_pass_pool{Pass::Params{div_size, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{div_size}, cover_pass_pool};
    Grid::Cover::Grid g = pool[c];
    g.build_if_stale();
    constexpr u32 k = 4;
    (void)g.ensure_octant(k);

    const auto dc = (int)g.div_count();
    const auto box = (int)Math::max(div_size, Pass::Grid::min_bbox_size);
    const auto first_blocked_row = (1083 + half_tile<int> - box/2 + 1) / (int)div_size;
    u32 hits = 0;
    for (const int off : { 0, dc / 8, -(dc / 8) })
        for (int y = Math::max(0, -off); y < Math::min(dc, dc - off); y++)
        {
            const int x = y + off;
            const auto uncapped_units = (float)(first_blocked_row - y) * Math::Constants<float>::sqrt2();
            const auto expected = Math::min((float)dc, uncapped_units);
            const auto d = g.distance(Grid::Cover::Grid::get_cell_index((u32)x, (u32)y, (u32)dc), k);
            if (Math::abs((float)d - expected) > .5f + 1.f/16)
            {
                Error{standard_error()} << "!!! fatal: div_size" << div_size << "cell" << x << y
                                        << "distance" << d << "but expected" << expected;
                fm_assert(false);
            }
            hits += uncapped_units < (float)dc;
        }
    fm_assert(hits >= (u32)dc / 4);
}

bool holds_object(const Grid::Object::Grid& g, object_id id)
{
    return any_of(g.entry_data(), [&](const auto& x) { return x.id == id; });
}

// The cells, as [min, max), of c's grid at div_size d where a box of bbox_size can touch o's box.
Range2Di cells_near(chunk& c, const object& o, int d, u32 bbox_size, int dc)
{
    const auto pos = o.position() - point{c.coord(), {}, {}};
    const auto off = Vector2i(o.bbox_offset), size = Vector2i(o.bbox_size);
    const auto pad = Math::max(Math::abs(off.x()), Math::abs(off.y()))
                   + (Math::max(size.x(), size.y()) + (int)bbox_size)/2 + d + 2;
    const auto lo = Math::max(Vector2i{0}, (pos - Vector2i{pad} + half_tile<Vector2i>) / d);
    const auto hi = Math::min(Vector2i{dc}, (pos + Vector2i{pad} + half_tile<Vector2i>) / d + Vector2i{1});
    return {lo, hi};
}

// Every cell of c within reach of changed_object is passable under p at each of its positions exactly when s holds
// it and o doesn't block it. Elsewhere o blocks nothing.
void check_cells(const char* what, chunk& c, const Pass::Params& params, const Pass::Grid& s,
                 Grid::Object::Grid& o, const char* pred_name, const Search::pred& p, Array<bool>& entry_blocked,
                 const object* changed_object)
{
    (void)o.blocked_objects(p, entry_blocked);
    fm_assert(!o.uses_u32());
    const auto d = (int)params.div_size(), dc = (int)s.div_count();
    const auto window = changed_object ? cells_near(c, *changed_object, d, params.bbox_size, dc) : Range2Di{};
    const auto nbs = c.world().neighbors(c.coord());
    for (int j = 0; j < dc; j++)
        for (int i = 0; i < dc; i++)
        {
            const auto k = Pass::Grid::get_bitmask_index((u32)i, (u32)j, (u32)dc);
            const bool blocked = o.is_blocked<u16>(k, entry_blocked);
            const bool checked = window.contains({i, j});
            const bool ok = checked ? (s.bit(k) && !blocked) == cell_is_passable(c, nbs, i, j, d, params.bbox_size, p,
                                                                                 params.min_pass)
                                    : !blocked;
            if (!ok)
            {
                ERR << "!!! fatal:" << what << pred_name << "min_pass" << (int)params.min_pass << "chunk"
                    << c.coord().x << c.coord().y << "cell" << i << j << "static" << s.bit(k) << "object" << blocked
                    << "near the object" << checked;
                fm_abort("the static and object grids disagree with Search::is_passable_");
            }
        }
}

void test_registry_grids_after_door_changes()
{
    constexpr auto cover_div_size = 32u;
    constexpr Pass::Params pass_params[] = {
        {16, 32, pass_mode::pass},
        {cover_div_size, pass_mode::shoot_through},
    };
    constexpr auto cover_params = Grid::Cover::Params{cover_div_size};
    constexpr u32 octants[] = { 0u, 4u, 8u, 12u, 16u, 20u, 24u, 28u };

    auto w = world();
    const auto for_chunks = [&](auto&& fun) {
        for (i16 y = -1; y <= 1; y++)
            for (i16 x = -1; x <= 2; x++)
                fun(w[chunk_coords_{x, y, 0}], x, y);
    };
    for_chunks([](chunk& c, i16, i16) { add_ground_all(c); });
    for (auto i : { 13, 14 })
        add_wall_north(w[COORD], {(u8)i, 8});
    for (auto i : { 0, 1, 3, 4 })
        add_wall_north(w[COORD_E], {(u8)i, 8});
    for_chunks([](chunk& c, i16, i16) { rebuild_passability(c); });

    Grid::GridRegistry reg;
    Pass::Pool unregistered[] = {
        Pass::Pool{pass_params[0]},
        Pass::Pool{pass_params[1]},
    };
    Array<bool> entry_blocked;
    const object* changed_object = nullptr;
    const auto check_pass = [&](const char* what) {
        Grid::GridRegistry fresh;
        for (auto j = 0u; const auto& p : pass_params)
        {
            auto& a = reg.pass_pool(p);
            auto& b = fresh.pass_pool(p);
            auto& objects = reg.object_pool(p);
            auto& kept = unregistered[j++];
            for_chunks([&](chunk& c, i16 x, i16 y) {
                Pass::Grid ga = a[c], gb = b[c], g_kept = kept[c];
                Grid::Object::Grid go = objects[c];
                for (auto* g : { &ga, &gb, &g_kept })
                    g->build_if_stale();
                go.build_if_stale();
                const auto dc = ga.div_count();
                for (auto i = 0u; i < dc*dc; i++)
                    if (ga.bit(i) != gb.bit(i) || g_kept.bit(i) != gb.bit(i))
                    {
                        ERR << "!!! fatal:" << what << "min_pass" << (int)p.min_pass << "chunk" << x << y
                            << "cell" << i % dc << i / dc << "static" << ga.bit(i) << "without_critters"
                            << g_kept.bit(i) << "but fresh" << gb.bit(i);
                        fm_abort("a pass grid differs from a fresh one");
                    }
                check_cells(what, c, p, ga, go, "without_critters", Search::without_critters, entry_blocked,
                            changed_object);
                check_cells(what, c, p, ga, go, "never_continue", Search::never_continue, entry_blocked,
                            changed_object);
            });
        }
    };
    const auto check = [&](const char* what) {
        check_pass(what);
        Grid::GridRegistry fresh;
        auto& a = reg.cover_pool(cover_params);
        auto& b = fresh.cover_pool(cover_params);
        for_chunks([&](chunk& c, i16 x, i16 y) {
            Grid::Cover::Grid ga = a[c], gb = b[c];
            ga.build_if_stale();
            gb.build_if_stale();
            const auto dc = ga.div_count();
            for (auto k : octants)
            {
                (void)ga.ensure_octant(k);
                (void)gb.ensure_octant(k);
                for (auto i = 0u; i < dc*dc; i++)
                    if (ga.distance(i, k) != gb.distance(i, k))
                    {
                        ERR << "!!! fatal:" << what << "chunk" << x << y
                            << "octant" << k << "cell" << i % dc << i / dc
                            << "distance" << ga.distance(i, k) << "but fresh" << gb.distance(i, k);
                        fm_abort("a cover grid differs from a fresh one");
                    }
            }
        });
    };
    // A door's changes stamp pass_gen(), so every grid within one chunk of a stamped one goes stale, and every cover
    // grid within two.
    const auto door_changed = [&](std::initializer_list<chunk_coords_> stamped) {
        for_chunks([&](chunk& c, i16 x, i16 y) {
            auto dist = (int)limits<i16>::max;
            for (auto s : stamped)
                dist = Math::min(dist, Math::max(Math::abs(x - s.x), Math::abs(y - s.y)));
            fm_assert_equal(dist == 0, c.is_passability_modified());
            for (auto j = 0u; const auto& p : pass_params)
            {
                fm_assert_equal(dist <= 1, reg.pass_pool(p)[c].is_stale());
                fm_assert_equal(dist <= 1, reg.object_pool(p)[c].is_stale());
                fm_assert_equal(dist <= 1, unregistered[j++][c].is_stale());
            }
            fm_assert_equal(dist <= 2, reg.cover_pool(cover_params)[c].is_stale());
        });
    };
    // A critter's changes stamp only objects_gen(), so pass and cover grids stay current.
    // Object grids go stale within one chunk of a stamped one.
    const auto changed = [&](std::initializer_list<chunk_coords_> stamped) {
        for_chunks([&](chunk& c, i16 x, i16 y) {
            fm_assert(!c.is_passability_modified());
            auto dist = (int)limits<i16>::max;
            for (auto s : stamped)
                dist = Math::min(dist, Math::max(Math::abs(x - s.x), Math::abs(y - s.y)));
            for (auto j = 0u; const auto& p : pass_params)
            {
                fm_assert(!reg.pass_pool(p)[c].is_stale());
                fm_assert_equal(dist <= 1, reg.object_pool(p)[c].is_stale());
                fm_assert(!unregistered[j++][c].is_stale());
            }
            fm_assert(!reg.cover_pool(cover_params)[c].is_stale());
        });
    };
    check("before the door");

    auto proto = loader.scenery("door1");
    proto.pass = pass_mode::blocked;
    proto.frame = (u16)(proto.atlas->info().nframes - 1);
    const auto doorʹ = w.make_scenery(w.make_id(), {COORD, local_coords{15, 8}}, move(proto));
    fm_assert(doorʹ->scenery_type() == scenery_type::door && doorʹ->is_dynamic());
    auto& door = static_cast<door_scenery&>(*doorʹ);
    changed_object = &door;
    door_changed({COORD});
    check("placed");

    auto i = door.index();
    door.teleport_to(i, {COORD, local_coords{12, 8}}, door.offset, door.r);
    door_changed({COORD});
    check("moved within the chunk");
    door.teleport_to(i, {COORD_E, local_coords{2, 8}}, door.offset, door.r);
    fm_assert(&door.chunk() == &w[COORD_E]);
    door_changed({COORD, COORD_E});
    check("moved into the east chunk");
    door.rotate(i, rotation::W);
    door_changed({COORD_E});
    check("turned");

    const auto toggle = [&] {
        fm_assert(door.activate(i));
        for (auto n = 0u; door.active; n++)
        {
            fm_assert(n < 100);
            door.update(doorʹ, i, Second);
        }
    };
    toggle();
    fm_assert(door.pass == pass_mode::pass);
    door_changed({COORD_E});
    check("opened");
    toggle();
    fm_assert(door.pass == pass_mode::blocked);
    door_changed({COORD_E});
    check("closed");

    door.chunk().remove_object(door, door.index());
    changed_object = nullptr;
    door_changed({COORD_E});
    check("removed");

    const auto pass_gen = w[COORD].pass_gen(), pass_gen_e = w[COORD_E].pass_gen();
    const auto objects_gen = w[COORD].objects_gen(), objects_gen_e = w[COORD_E].objects_gen();
    critter_proto cp;
    cp.name = "npc"_s;
    const auto npc = w.make_object<critter>(w.make_id(), {COORD, local_coords{15, 8}}, move(cp));
    changed_object = &*npc;
    changed({COORD});
    check_pass("critter placed");
    for (const auto& p : pass_params)
        fm_assert(holds_object(reg.object_pool(p)[w[COORD]], npc->id));
    i = npc->index();
    npc->teleport_to(i, {COORD_E, local_coords{2, 8}}, {}, npc->r);
    changed({COORD, COORD_E});
    check_pass("critter moved");
    fm_assert(w[COORD].pass_gen() == pass_gen && w[COORD_E].pass_gen() == pass_gen_e);
    fm_assert(w[COORD].objects_gen() > objects_gen && w[COORD_E].objects_gen() > objects_gen_e);
}

// A critter's changes stamp only objects_gen(), so pass and cover grids stay current, and object grids go stale
// within one chunk of a stamped one.
void test_grids_after_critter_changes()
{
    constexpr auto params = Pass::Params{16, 32, pass_mode::pass};
    auto w = world();
    const auto for_chunks = [&](auto&& fun) {
        for (i16 y = -1; y <= 1; y++)
            for (i16 x = -2; x <= 3; x++)
                fun(w[chunk_coords_{x, y, 0}], x, y);
    };
    for_chunks([](chunk& c, i16, i16) { add_ground_all(c); rebuild_passability(c); });

    Grid::GridRegistry reg;
    auto& static_pool = reg.pass_pool(params);
    auto& static_cover = reg.cover_pool(Grid::Cover::Params{16});
    auto& objects = reg.object_pool(params);
    Pass::Pool critter_pool{params};
    Pass::Pool critter_cover_pass{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool critter_cover{Grid::Cover::Params{16}, critter_cover_pass};
    Array<bool> entry_blocked;
    const object* changed_object = nullptr;
    const auto build = [&] {
        for (auto* pool : { &static_pool, &critter_pool })
            for_chunks([&](chunk& c, i16, i16) { (*pool)[c].build_if_stale(); });
        for (auto* pool : { &static_cover, &critter_cover })
            for_chunks([&](chunk& c, i16, i16) { (*pool)[c].build_if_stale(); });
        for_chunks([&](chunk& c, i16, i16) { objects[c].build_if_stale(); });
    };
    const auto changed = [&](const char* what, std::initializer_list<chunk_coords_> stamped) {
        for_chunks([&](chunk& c, i16 x, i16 y) {
            auto dist = (int)limits<i16>::max;
            for (auto s : stamped)
                dist = Math::min(dist, Math::max(Math::abs(x - s.x), Math::abs(y - s.y)));
            fm_assert(!static_pool[c].is_marked_stale() && !static_pool[c].is_stale());
            fm_assert(!static_cover[c].is_marked_stale() && !static_cover[c].is_stale());
            fm_assert(!critter_pool[c].is_marked_stale() && !critter_pool[c].is_stale());
            fm_assert(!critter_cover[c].is_marked_stale() && !critter_cover[c].is_stale());
            fm_assert(!objects[c].is_marked_stale());
            fm_assert_equal(dist <= 1, objects[c].is_stale());
        });
        build();
        for_chunks([&](chunk& c, i16 x, i16 y) {
            auto a = critter_pool[c], s = static_pool[c];
            auto o = objects[c];
            fm_assert(!o.is_stale());
            const auto dc = a.div_count();
            for (auto i = 0u; i < dc*dc; i++)
                if (a.bit(i) != s.bit(i))
                {
                    ERR << "!!! fatal:" << what << "chunk" << x << y << "cell" << i % dc << i / dc
                        << "never_continue" << a.bit(i) << "but static" << s.bit(i);
                    fm_abort("a pass grid differs from the static one");
                }
            check_cells(what, c, params, s, o, "never_continue", Search::never_continue, entry_blocked,
                        changed_object);
        });
    };
    build();

    critter_proto cp;
    cp.name = "npc"_s;
    cp.bbox_size = {24, 12};
    const auto npc = w.make_object<critter>(w.make_id(), {COORD, local_coords{15, 8}}, move(cp));
    changed_object = &*npc;
    changed("placed", {COORD});
    auto i = npc->index();
    npc->teleport_to(i, {COORD, local_coords{12, 8}}, {}, npc->r);
    changed("moved within the chunk", {COORD});
    npc->teleport_to(i, {COORD, local_coords{15, 8}}, {24, 0}, npc->r);
    changed("box moved across the east border", {COORD});
    fm_assert(holds_object(objects[w[COORD_E]], npc->id));
    npc->teleport_to(i, {COORD_E, local_coords{2, 8}}, {}, npc->r);
    fm_assert(&npc->chunk() == &w[COORD_E]);
    changed("moved into the east chunk", {COORD, COORD_E});
    npc->rotate(i, rotation::W);
    changed("turned", {COORD_E});
    npc->chunk().remove_object(*npc, npc->index());
    changed_object = nullptr;
    changed("removed", {COORD_E});
}

bptr<critter> add_critter_at(world& w, Vector2i px, u8 size, pass_mode pass)
{
    const auto pt = point::normalize_coords(point{COORD, {}, {}}, px);
    critter_proto p;
    p.name = "npc"_s;
    p.bbox_size = Vector2ub{size};
    p.offset = pt.offset();
    p.pass = pass;
    auto C = w.make_object<critter>(w.make_id(), pt.coord(), move(p));
    fm_assert(C->position() == pt);
    return C;
}

struct placed_critter
{
    Vector2i px;
    u8 size;
    pass_mode pass = pass_mode::blocked;
};

u8 entry_chunk_of(chunk_coords_ ch)
{
    const auto off = Vector2b{(i8)(ch.x - COORD.x), (i8)(ch.y - COORD.y)};
    if (off == Vector2b{})
        return 0;
    for (auto n = 0u; n < 8; n++)
        if (world::neighbor_offsets[n] == off)
            return (u8)(1 + n);
    fm_abort("chunk isn't a neighbor of COORD");
}

u32 index_of(ArrayView<const object_id> ids, object_id id)
{
    auto n = 0u;
    while (n < ids.size() && ids[n] != id)
        n++;
    fm_assert(n < ids.size());
    return n;
}

// For each entry of g, the index in ids of the entry's object.
void entry_placement(const Grid::Object::Grid& g, ArrayView<const object_id> ids, Array<u32>& out)
{
    arrayClear(out);
    arrayReserve(out, g.entry_data().size());
    for (const auto& x : g.entry_data())
        arrayAppend(out, index_of(ids, x.id));
}

// The indices in ids of the objects covering cell i, ascending.
template<typename T>
void objects_in_cell(const Grid::Object::Grid& g, u32 i, ArrayView<const object_id> ids, Array<u32>& out)
{
    arrayClear(out);
    for (const auto& x : g.objects_at_cell<T>(i))
        arrayAppend(out, index_of(ids, x.id));
    std::sort(out.begin(), out.end());
}

void objects_in_cell(const Grid::Object::Grid& g, u32 i, ArrayView<const object_id> ids, Array<u32>& out)
{
    if (g.uses_u32())
        objects_in_cell<u32>(g, i, ids, out);
    else
        objects_in_cell<u16>(g, i, ids, out);
}

// Every cell of COORD's object grid against Search::is_passable_ at every position the cell holds, with a
// predicate blocking one critter at a time. Positions are in COORD's frame.
void check_object_grid(const char* what, u32 div_size, u32 bbox_size, std::initializer_list<placed_critter> placed,
                       bool overlapping)
{
    auto w = world();
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
            add_ground_all(w[chunk_coords_{x, y, 0}]);
    Array<object_id> ids;
    Array<u8> chunks;
    arrayReserve(ids, placed.size());
    arrayReserve(chunks, placed.size());
    for (const auto& p : placed)
    {
        const auto C = add_critter_at(w, p.px, p.size, p.pass);
        arrayAppend(ids, C->id);
        arrayAppend(chunks, entry_chunk_of(C->chunk().coord()));
    }
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
            rebuild_passability(w[chunk_coords_{x, y, 0}]);

    Pass::Pool pass_pool{Pass::Params{div_size, bbox_size, pass_mode::pass}};
    Grid::Object::Pool pool{pass_pool};
    auto& c = w[COORD];
    Grid::Object::Grid g = pool[c];
    g.build_if_stale();
    fm_assert(!g.uses_u32());
    const int dc = (int)g.div_count(), d = (int)div_size;
    const auto nbs = w.neighbors(COORD);

    auto expected = Array<u32>{ValueInit, (usize)(dc*dc)};
    u32 covering = 0;
    for (const auto& [k, id] : enumerate(ids))
    {
        const auto only = [id](const chunk&, collision_data x) {
            return x.id == id ? path_search_continue::blocked : path_search_continue::pass;
        };
        const Search::pred pred{only};
        const auto& p = placed.begin()[k];
        const int pad = (int)bbox_size + 2*d + p.size;
        const int i_lo = Math::max(0, (p.px.x() - pad + half_tile<int>) / d),
                  i_hi = Math::min(dc - 1, (p.px.x() + pad + half_tile<int>) / d),
                  j_lo = Math::max(0, (p.px.y() - pad + half_tile<int>) / d),
                  j_hi = Math::min(dc - 1, (p.px.y() + pad + half_tile<int>) / d);
        for (int j = j_lo; j <= j_hi; j++)
            for (int i = i_lo; i <= i_hi; i++)
            {
                if (!cell_is_passable(c, nbs, i, j, d, bbox_size, pred))
                {
                    expected[(usize)(j*dc + i)] |= 1u << k;
                    covering |= 1u << k;
                }
            }
    }

    Array<u32> placement, objects;
    arrayReserve(objects, 16);
    entry_placement(g, ids, placement);
    bool any_shared = false;
    for (auto idx = 0u; idx < (u32)(dc*dc); idx++)
    {
        objects_in_cell(g, idx, ids, objects);
        u32 mask = 0;
        for (auto k : objects)
            mask |= 1u << k;
        any_shared |= objects.size() >= 2;
        if (mask != expected[idx] || objects.size() != (u32)std::popcount(mask))
        {
            ERR << "!!! fatal:" << what << "div_size" << d << "bbox_size" << bbox_size << "cell" << idx % (u32)dc
                << idx / (u32)dc << "holds" << objects.size() << "objects" << mask << "but positions say"
                << expected[idx];
            fm_abort("an object grid cell differs from its positions");
        }
    }
    fm_assert(any_shared == overlapping);
    fm_assert(placement.size() == (u32)std::popcount(covering));
    for (const auto& [e, k] : enumerate(placement))
    {
        fm_assert(covering & 1u << k);
        fm_assert(g.entry_chunks()[e] == chunks[k]);
    }

    const auto words = Grid::detail::bitmask_size((u32)dc) / (sizeof(usize)*8);
    const auto all_set = Array<usize>{DirectInit, words, ~usize{0}};
    auto masked = Array<usize>{NoInit, words};
    Array<bool> entry_blocked;
    for (const auto& [k, id] : enumerate(ids))
    {
        const auto only = [id](const chunk&, collision_data x) {
            return x.id == id ? path_search_continue::blocked : path_search_continue::pass;
        };
        const Search::pred pred{only};
        fm_assert(g.blocked_objects(pred, entry_blocked) == !!(covering & 1u << k));
        fm_assert(g.blocked_objects(pred) == !!(covering & 1u << k));
        g.mask_pass_bits(all_set, masked, entry_blocked);
        for (auto idx = 0u; idx < (u32)(dc*dc); idx++)
        {
            const bool blocked = expected[idx] & 1u << k;
            if (g.is_blocked<u16>(idx, entry_blocked) != blocked || pass_bit(masked.data(), idx) == blocked)
            {
                ERR << "!!! fatal:" << what << "div_size" << d << "bbox_size" << bbox_size << "cell" << idx % (u32)dc
                    << idx / (u32)dc << "critter" << k << "blocks" << g.is_blocked<u16>(idx, entry_blocked)
                    << "masked" << pass_bit(masked.data(), idx) << "but positions say" << blocked;
                fm_abort("an object grid cell blocks differently from its positions");
            }
        }
    }
}

void test_object_grid_matches_positions(u32 div_size, u32 bbox_size)
{
    check_object_grid("one critter", div_size, bbox_size, { {{300, 400}, 32} }, false);
    check_object_grid("two overlapping critters", div_size, bbox_size,
                      { {{300, 400}, 32}, {{330, 410}, 24} }, true);
    check_object_grid("pass and see-through critters", div_size, bbox_size,
                      { {{300, 400}, 32, pass_mode::pass}, {{600, 600}, 32, pass_mode::see_through} }, false);
    check_object_grid("critters in neighbor chunks", div_size, bbox_size,
                      { {{1000, 400}, 32}, {{-40, -40}, 24}, {{500, 1010}, 20} }, false);
    check_object_grid("no critters", div_size, bbox_size, {}, false);
}

void check_same_cells(const char* what, const Grid::Object::Grid& g, chunk& c, const Pass::Params& params,
                      ArrayView<const object_id> ids)
{
    Pass::Pool pass_pool{params};
    Grid::Object::Pool pool{pass_pool};
    Grid::Object::Grid fresh = pool[c];
    fresh.build_if_stale();
    fm_assert(g.entry_data().size() == fresh.entry_data().size());
    const auto dc = g.div_count();
    Array<u32> a, b;
    arrayReserve(a, 16);
    arrayReserve(b, 16);
    for (auto i = 0u; i < dc*dc; i++)
    {
        objects_in_cell(g, i, ids, a);
        objects_in_cell(fresh, i, ids, b);
        bool same = a.size() == b.size();
        for (auto k = 0u; same && k < a.size(); k++)
            same = a[k] == b[k];
        if (!same)
        {
            ERR << "!!! fatal:" << what << "cell" << i % dc << i / dc << "holds" << a.size() << "objects but"
                << b.size() << "in a fresh grid";
            fm_abort("a rebuilt object grid differs from a fresh one");
        }
    }
}

// A critter on every tile needs more run entries than u16 cells can index. Critters two tiles apart share no
// cell, so a u16 grid of each such set says which cells each critter covers.
void test_object_grid_crowd()
{
    constexpr auto params = Pass::Params{8, 32, pass_mode::pass};
    constexpr auto n = (u32)TILE_COUNT;
    constexpr auto size = (u8)(tile_size_xy*3/2);
    const auto tile_px = [](u32 k) {
        return Vector2i{(int)(k % TILE_MAX_DIM), (int)(k / TILE_MAX_DIM)} * tile_size_xy;
    };
    const auto tile_parity = [](u32 k) { return k % 2 + k / TILE_MAX_DIM % 2 * 2; };

    auto w = world();
    Array<bptr<critter>> crowd;
    Array<object_id> ids;
    arrayReserve(crowd, n);
    arrayReserve(ids, n);
    for (auto k = 0u; k < n; k++)
    {
        const auto C = add_critter_at(w, tile_px(k), size, pass_mode::blocked);
        arrayAppend(crowd, C);
        arrayAppend(ids, C->id);
    }
    Pass::Pool pass_pool{params};
    Grid::Object::Pool pool{pass_pool};
    Grid::Object::Grid g = pool[w[COORD]];
    g.build_if_stale();
    Array<u32> placement, objects;
    arrayReserve(objects, 16);
    entry_placement(g, ids, placement);
    fm_assert(placement.size() == n);

    const auto dc = g.div_count();
    auto expected = Array<u32>{DirectInit, dc*dc*4, (u32)-1};
    for (auto s = 0u; s < 4; s++)
    {
        auto ws = world();
        auto ids_s = Array<object_id>{ValueInit, n};
        for (auto k = 0u; k < n; k++)
            if (tile_parity(k) == s)
                ids_s[k] = add_critter_at(ws, tile_px(k), size, pass_mode::blocked)->id;
        Grid::Object::Pool pool_s{pass_pool};
        Grid::Object::Grid gs = pool_s[ws[COORD]];
        gs.build_if_stale();
        fm_assert(!gs.uses_u32());
        for (auto i = 0u; i < dc*dc; i++)
        {
            objects_in_cell(gs, i, ids_s, objects);
            fm_assert(objects.size() <= 1);
            if (!objects.isEmpty())
                expected[i*4 + s] = objects[0];
        }
    }
    for (auto i = 0u; i < dc*dc; i++)
    {
        auto* e = expected.data() + i*4;
        std::sort(e, e + 4);
        auto count = 0u;
        while (count < 4 && e[count] != (u32)-1)
            count++;
        objects_in_cell(g, i, ids, objects);
        bool same = objects.size() == count;
        for (auto k = 0u; same && k < count; k++)
            same = objects[k] == e[k];
        if (!same)
        {
            ERR << "!!! fatal: cell" << i % dc << i / dc << "holds" << objects.size() << "objects but" << count;
            fm_abort("a crowded object grid differs from the grids of its critters two tiles apart");
        }
    }
    fm_assert(g.uses_u32());

    const auto words = Grid::detail::bitmask_size(dc) / (sizeof(usize)*8);
    const auto all_set = Array<usize>{DirectInit, words, ~usize{0}};
    auto masked = Array<usize>{NoInit, words};
    Array<bool> entry_blocked;
    fm_assert(g.blocked_objects(Search::never_continue, entry_blocked));
    g.mask_pass_bits(all_set, masked, entry_blocked);
    for (auto i = 0u; i < dc*dc; i++)
    {
        const bool covered = expected[i*4] != (u32)-1;
        if (pass_bit(masked.data(), i) == covered)
        {
            ERR << "!!! fatal: cell" << i % dc << i / dc << "masked" << pass_bit(masked.data(), i)
                << "but covered" << covered;
            fm_abort("a crowded object grid masks pass bits differently from its cells");
        }
    }

    for (auto k = 0u; k < n; k++)
        if (k % 64 != 0)
            crowd[k]->chunk().remove_object(*crowd[k], crowd[k]->index());
    g.build_if_stale();
    fm_assert(g.uses_u32());
    check_same_cells("after the crowd went", g, w[COORD], params, ids);
    crowd[64]->chunk().remove_object(*crowd[64], crowd[64]->index());
    g.build_if_stale();
    fm_assert(!g.uses_u32());
    check_same_cells("after one more went", g, w[COORD], params, ids);
}

void test_object_grid_rebuild()
{
    constexpr auto params = Pass::Params{8, 32, pass_mode::pass};
    Pass::Pool pass_pool{params};
    Grid::Object::Pool pool{pass_pool};
    Array<object_id> ids;
    arrayReserve(ids, 5);
    {
        auto w = world();
        const auto A = add_critter_at(w, {300, 400}, 32, pass_mode::blocked);
        const auto B = add_critter_at(w, {330, 410}, 24, pass_mode::blocked);
        const auto C = add_critter_at(w, {600, 600}, 32, pass_mode::blocked);
        arrayAppend(ids, A->id);
        arrayAppend(ids, B->id);
        arrayAppend(ids, C->id);
        Grid::Object::Grid g = pool[w[COORD]];
        g.build_if_stale();
        auto i = A->index();
        A->teleport_to(i, point::normalize_coords(point{COORD, {}, {}}, Vector2i{800, 200}), A->r);
        B->chunk().remove_object(*B, B->index());
        fm_assert(g.is_stale());
        g.build_if_stale();
        check_same_cells("after a critter moved and one went", g, w[COORD], params, ids);
    }
    fm_assert(pool.free_grid_count() == 1);
    auto w = world();
    arrayAppend(ids, add_critter_at(w, {500, 200}, 32, pass_mode::blocked)->id);
    arrayAppend(ids, add_critter_at(w, {520, 230}, 24, pass_mode::blocked)->id);
    Grid::Object::Grid g = pool[w[COORD]];
    fm_assert(pool.free_grid_count() == 0);
    g.build_if_stale();
    check_same_cells("in a grid reused for another chunk", g, w[COORD], params, ids);
}

} // namespace

void test_grid()
{
    for (const auto ds : { 16u, 8u })
    {
        test_ground_only_is_fully_passable(ds);
        test_wall_blocks_some_cells(ds);
        test_idempotent_rebuild(ds);
        test_neighbor_cascade(ds);
        test_collect_cleanup(ds);
        test_add_remove_wall_restores_baseline(ds);
        test_neighbor_west_wall_affects_east_edge(ds);
        for (const auto bbox : { ds, chunk_size_xy })
            test_pass_blocker_leaves_ring2_current(ds, bbox);
        test_bit_from_tile_center_passable(ds);
        test_wall_then_clear_ground_allows_collect(ds);
        test_cell_at_wall_is_blocked(ds);
        test_cell_south_of_wall_is_passable(ds);
        test_all_chunk_corners_passable(ds);
        test_div_count_derived_from_div_size(ds);
        test_blocked_ground_blocks_all(ds);
        test_multi_chunk_3x3_sanity(ds);
        test_chunk_reinsertion_after_collect(ds);
        test_frame_counter_ticks_independently(ds);
        test_empty_pool_safe_tick(ds);
        test_last_bit_index_valid(ds);
        test_wall_west_blocks(ds);
        test_many_walls_strictly_reduce_passable(ds);
        test_repeated_ticks_noop_on_stable_world(ds);
        test_explicit_wrapper_mark_stale_rebuilds(ds);
        test_mark_modified_only_without_ensure_passability(ds);
        test_neighbor_mark_only_without_ensure_detected(ds);
        test_rebuild_after_far_change_reads_as_stale(ds);
        test_params_preserved_through_pool(ds);
        test_collect_all_chunks_no_crash(ds);
        test_pool_reuse_after_collect(ds);
        test_pool_reuse_neighbors_collected(ds);
        test_neighbor_lifecycle(ds);
        test_self_collect_with_neighbor(ds);
        test_pool_reuse_across_different_coord(ds);
        test_mark_and_ensure_preserve_frame_no(ds);
        test_multiple_free_grids(ds);
        test_bitview_read_matches_bit(ds);
        test_partial_collect_then_collect_survivors(ds);
    }
    test_pool_destruction_with_live_grids();
    test_chunk_pass_gen_unique_after_collect();
    test_chunk_pass_gen_monotonic_after_move_ctor();
    test_chunk_pass_gen_monotonic_after_move_assign();
    test_grids_after_world_move_ctor();
    test_world_move_ctor_from_moved_from();
    test_grids_dropped_by_world_move_assign();
    test_world_assign_into_moved_from();
    for (const auto ds : { 1u, 2u, 4u, 16u, 64u })
        test_cell_spans_match_forward_map(ds);
    for (const auto min_pass : { pass_mode::pass, pass_mode::shoot_through })
        for (const auto& cfg : exact_configs)
            test_bit_exact(cfg, min_pass);
    for (const auto ds : { 4u, 16u })
    {
        for (const auto p : { pass_mode::blocked, pass_mode::see_through,
                              pass_mode::shoot_through, pass_mode::pass })
            test_hole_marker_never_blocks(ds, p);
        test_hole_still_opens_blocked_ground(ds);
    }
    u32 all_octants[Grid::Cover::octant_count], non_octilinear_octants[Grid::Cover::octant_count - 8];
    for (auto k = 0u, n = 0u; k < Grid::Cover::octant_count; k++)
    {
        all_octants[k] = k;
        if (k % 4 != 0)
            non_octilinear_octants[n++] = k;
    }
    test_cover_octant_directions();
    // Below 8, planes are u16. One octant of each step, turned and mirrored, keeps the tests short there.
    constexpr u32 some_octants[] = { 0u, 1u, 4u, 6u, 8u, 12u, 13u, 16u, 20u, 22u, 24u, 27u, 28u };
    constexpr u32 three_octants[] = { 0u, 1u, 4u };
    for (const i64 target : hwy::SupportedAndGeneratedTargets())
    {
        hwy::SetSupportedTargetsForTest(target);
        test_expand_pass_row();
        for (const auto ds : { 1u, 2u, 8u })
            test_cover_east_distance(ds);
        for (const auto ds : { 1u, 2u, 4u, 8u, 16u })
        {
            test_cover_east_distance_across_border(ds);
            test_cover_diagonal_sums_steps_across_border(ds);
            const auto octants = ds < 4 ? ArrayView<const u32>{three_octants}
                               : ds < 8 ? ArrayView<const u32>{some_octants} : all_octants;
            test_cover_distance_never_exceeds_ray(ds, octants);
        }
        // 4 has u16 planes
        constexpr u32 first_steps[] = { 1u, 2u };
        for (const auto ds : { 4u, 8u, 16u })
        {
            test_cover_non_octilinear_octant_blocked_cells(ds, ds < 8 ? ArrayView<const u32>{first_steps}
                                                                      : non_octilinear_octants);
            test_cover_ignores_chunk_borders(ds, ds < 8 ? ArrayView<const u32>{some_octants} : all_octants);
        }
        for (const auto ds : { 8u, 16u })
            test_cover_blocked_cells(ds);
        // sizes below 16 add ~17 s under ASan
        const auto min_size = fm_ASAN ? 16u : is_valgrind() ? 4u : 1u;
        for (const auto ds : { 1u, 2u, 4u, 8u, 16u, 32u, 64u })
            if (ds >= min_size)
                test_cover_matches_reference(ds, ds < 8 ? ArrayView<const u32>{some_octants} : all_octants);
    }
    hwy::SetSupportedTargetsForTest(0);
    vqsort_refresh_targets();
    // At div 2 and 16, octant 6's fill reads a row past the farthest one that can lower a distance.
    // Octants 2 and 7 don't.
    for (const auto ds : { 1u, 2u, 4u, 8u, 16u, 32u, 64u })
        test_cover_reads_farthest_neighbor_row(ds, ds == 2 ? 2u : ds == 16 ? 7u : 6u);
    for (const auto ds : { 8u, 16u })
    {
        test_cover_world_edge(ds);
        test_cover_missing_neighbors_read_as_open_ground(ds);
    }
    test_cover_rebuilds_on_neighbor_change(16);
    for (const auto ds : { 8u, 16u })
    {
        test_cover_rebuilds_on_ring2_change(ds);
        test_cover_rebuilds_on_ring2_collect(ds);
        test_cover_overhang_into_missing_chunk_reads_open(ds);
    }
    test_is_stale_on_chunk_change();
    test_hole_change_leaves_far_grids_current();
    test_cover_grids_after_static_changes();
    test_is_stale_on_chunk_create_and_collect();
    test_is_stale_lifecycle();
    test_cover_octant_fills_include_rebuild();
    test_registry_grids_after_door_changes();
    test_grids_after_critter_changes();
    test_object_grid_matches_positions(8, 32);
    test_object_grid_matches_positions(16, 16);
    test_object_grid_matches_positions(4, 7);
    test_object_grid_crowd();
    test_object_grid_rebuild();
}

} // namespace floormat::Test
