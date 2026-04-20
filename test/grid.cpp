#include "app.hpp"
#include "src/grid-pass.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"

namespace floormat::Test {

namespace {

constexpr auto chunk_size_xy = tile_size_xy * (int32_t)TILE_MAX_DIM;

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

void tick(world& w, Pass::Pool& pool)
{
    const auto frame = w.frame_no();
    pool.maybe_mark_stale_all(frame);
    pool.build_if_stale_all();
}

uint32_t count_passable(const Pass::Grid& g)
{
    const auto dc = g.div_count();
    uint32_t n = 0;
    for (auto j = 0u; j < dc; j++)
        for (auto i = 0u; i < dc; i++)
            if (g.bit(Pass::Grid::get_bitmask_index(i, j, dc)))
                n++;
    return n;
}

uint32_t count_passable_built(Pass::Pool& pool, chunk& c)
{
    Pass::Grid g = pool[c];
    g.build_if_stale();
    return count_passable(g);
}

void test_ground_only_is_fully_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);

    Pass::Grid grid = pool[c];
    grid.build_if_stale();

    const auto dc = grid.div_count();
    fm_assert(dc == (uint32_t)chunk_size_xy / div_size);
    fm_assert(count_passable(grid) == dc * dc);
}

void test_wall_blocks_some_cells(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    {
        Pass::Grid grid = pool[c];
        grid.build_if_stale();
        const auto dc = grid.div_count();
        fm_assert(count_passable(grid) == dc * dc);
    }

    add_wall_north(c, {8, 8});
    rebuild_passability(c);

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

void test_idempotent_rebuild(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid grid = pool[c];
    grid.build_if_stale();
    const auto first = count_passable(grid);

    grid.mark_stale();
    grid.build_if_stale();
    const auto second = count_passable(grid);
    fm_assert(first == second);
}

void test_neighbor_cascade(uint32_t div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g0 = pool[c0];
    Pass::Grid g1 = pool[c1];
    g0.build_if_stale();
    g1.build_if_stale();
    const auto base1 = count_passable(g1);

    add_wall_north(c1, {0, 0});
    rebuild_passability(c1);

    tick(w, pool);
    Pass::Grid g1ʹ = pool[c1];
    const auto after1 = count_passable(g1ʹ);
    fm_assert(after1 < base1);
}

void test_collect_cleanup(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
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

void test_add_remove_wall_restores_baseline(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto baseline = count_passable_built(pool, c);

    add_wall_north(c, {8, 8});
    rebuild_passability(c);
    tick(w, pool);
    const auto with_wall = count_passable_built(pool, c);
    fm_assert(with_wall < baseline);

    c[{8, 8}].wall_north() = wall_image_proto{};
    rebuild_passability(c);
    tick(w, pool);
    const auto restored = count_passable_built(pool, c);
    fm_assert(restored == baseline);
}

void test_neighbor_west_wall_affects_east_edge(uint32_t div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base0 = count_passable_built(pool, c0);

    for (auto y = 0u; y < TILE_MAX_DIM; y++)
        c1[{0, (uint8_t)y}].wall_west() = wall_proto();
    rebuild_passability(c1);

    tick(w, pool);
    const auto after0 = count_passable_built(pool, c0);
    fm_assert(after0 < base0);
}

void test_bit_from_tile_center_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid grid = pool[c];
    grid.build_if_stale();

    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
        {
            const auto idx = grid.get_bitmask_index_from_coord(local_coords{(uint8_t)i, (uint8_t)j}, Vector2b{0, 0});
            fm_assert(grid.bit(idx));
        }
}

void test_wall_then_clear_ground_allows_collect(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { const auto passable = count_passable_built(pool, c);
      fm_assert(passable > 0); }

    clear_ground_all(c);
    c[{4, 4}].wall_north() = wall_image_proto{};
    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    tick(w, pool);
}

void test_cell_at_wall_is_blocked(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {8, 8});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto idx = g.get_bitmask_index_from_coord(local_coords{8, 7}, Vector2b{0, 28});
    fm_assert(!g.bit(idx));
}

void test_cell_south_of_wall_is_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {8, 8});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto idx = g.get_bitmask_index_from_coord(local_coords{8, 10}, Vector2b{0, 0});
    fm_assert(g.bit(idx));
}

void test_all_chunk_corners_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const local_coords corners[] = { {0, 0}, {15, 0}, {0, 15}, {15, 15} };
    for (const auto lc : corners)
    {
        const auto idx = g.get_bitmask_index_from_coord(lc, Vector2b{0, 0});
        fm_assert(g.bit(idx));
    }
}

void test_div_count_derived_from_div_size(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    fm_assert(g.div_count() == (uint32_t)chunk_size_xy / div_size);
    fm_assert(g.div_count() * div_size == (uint32_t)chunk_size_xy);
}

void test_pool_destruction_with_live_grids()
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    {
        Pass::Pool pool{Pass::Params{16}};
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

void test_blocked_ground_blocks_all(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_blocked_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();
    fm_assert(count_passable(g) == 0);
}

void test_multi_chunk_3x3_sanity(uint32_t div_size)
{
    auto w = world();
    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = w[chunk_coords_{(int16_t)x, (int16_t)y, 0}];
            add_ground_all(c);
            rebuild_passability(c);
        }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = *w.at(chunk_coords_{(int16_t)x, (int16_t)y, 0});
            Pass::Grid g = pool[c];
            g.build_if_stale();
            const auto dc = g.div_count();
            fm_assert(count_passable(g) == dc * dc);
        }
}

void test_chunk_reinsertion_after_collect(uint32_t div_size)
{
    auto w = world();

    {
        auto& c = w[COORD];
        add_ground_all(c);
        rebuild_passability(c);
    }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { Pass::Grid g = pool[*w.at(COORD)];
      g.build_if_stale();
      fm_assert(count_passable(g) > 0); }

    clear_ground_all(*w.at(COORD));
    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    tick(w, pool);

    auto& c2 = w[COORD];
    add_ground_all(c2);
    rebuild_passability(c2);

    tick(w, pool);
    Pass::Grid g2 = pool[c2];
    g2.build_if_stale();
    const auto dc = g2.div_count();
    fm_assert(count_passable(g2) == dc * dc);
}

void test_frame_counter_ticks_independently(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
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

void test_empty_pool_safe_tick(uint32_t div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    tick(w, pool);
}

void test_last_bit_index_valid(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto dc = g.div_count();
    const auto last_idx = Pass::Grid::get_bitmask_index(dc - 1, dc - 1, dc);
    fm_assert(g.bit(last_idx));
}

void test_wall_west_blocks(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    c[{8, 8}].wall_west() = wall_proto();
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto dc = g.div_count();
    fm_assert(count_passable(g) < dc * dc);
    fm_assert(count_passable(g) > 0);
}

void test_many_walls_strictly_reduce_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);

    add_wall_north(c, {4, 4});
    rebuild_passability(c);
    tick(w, pool);
    const auto one = count_passable_built(pool, c);
    fm_assert(one < base);

    add_wall_north(c, {8, 8});
    rebuild_passability(c);
    tick(w, pool);
    const auto two = count_passable_built(pool, c);
    fm_assert(two < one);
}

void test_repeated_ticks_noop_on_stable_world(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {5, 5});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto first = count_passable_built(pool, c);

    for (int i = 0; i < 5; i++)
    {
        tick(w, pool);
        fm_assert(count_passable_built(pool, c) == first);
    }
}

void test_explicit_wrapper_mark_stale_rebuilds(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);

    Pass::Grid g = pool[c];
    g.mark_stale();
    g.build_if_stale();
    fm_assert(count_passable(g) == base);
}

void test_mark_modified_only_without_ensure_passability(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);

    add_wall_north(c, {8, 8});
    c.mark_passability_modified();

    tick(w, pool);
    const auto after = count_passable_built(pool, c);
    fm_assert(after < base);
}

void test_neighbor_mark_only_without_ensure_detected(uint32_t div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base0 = count_passable_built(pool, c0);

    for (auto y = 0u; y < TILE_MAX_DIM; y++)
        c1[{0, (uint8_t)y}].wall_west() = wall_proto();
    c1.mark_passability_modified();

    tick(w, pool);
    const auto after0 = count_passable_built(pool, c0);
    fm_assert(after0 < base0);
}

void test_params_preserved_through_pool(uint32_t div_size)
{
    Pass::Pool pool{Pass::Params{div_size}};
    const auto p = pool.params();
    fm_assert(p.div_size == div_size);
    fm_assert(p.bbox_size == tile_size_xy);
}

void test_collect_all_chunks_no_crash(uint32_t div_size)
{
    auto w = world();
    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = w[chunk_coords_{(int16_t)x, (int16_t)y, 0}];
            add_ground_all(c);
            rebuild_passability(c);
        }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
            { auto& c = *w.at(chunk_coords_{(int16_t)x, (int16_t)y, 0});
              (void)count_passable_built(pool, c); }

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
            clear_ground_all(*w.at(chunk_coords_{(int16_t)x, (int16_t)y, 0}));

    w.collect(true, true);
    fm_assert(w.size() == 0);

    tick(w, pool);
}

void test_pool_reuse_after_collect(uint32_t div_size)
{
    constexpr chunk_coords_ C11{1, 1, 0};
    auto w = world();
    auto& c = w[C11];
    add_wall_north(c, {0, 0});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { Pass::Grid g = pool[c]; g.build_if_stale(); }

    const auto before = pool.pooled_count();

    c[{0, 0}].wall_north() = wall_image_proto{};
    c.mark_passability_modified();
    w.collect(true, true);
    fm_assert(w.at(C11) == nullptr);

    pool.maybe_mark_stale_all(w.frame_no());
    fm_assert(pool.pooled_count() == before + 1);
}

void test_pool_reuse_neighbors_collected(uint32_t div_size)
{
    constexpr chunk_coords_ A{1, 1, 0};
    constexpr chunk_coords_ B{2, 1, 0};
    auto w = world();
    { auto& ca = w[A]; add_wall_north(ca, {0, 0}); rebuild_passability(ca); }
    { auto& cb = w[B]; add_wall_north(cb, {0, 0}); rebuild_passability(cb); }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { (void)pool[*w.at(A)]; (void)pool[*w.at(B)]; }

    const auto before = pool.pooled_count();

    { auto& ca = *w.at(A);
      ca[{0, 0}].wall_north() = wall_image_proto{};
      ca.mark_passability_modified(); }
    { auto& cb = *w.at(B);
      cb[{0, 0}].wall_north() = wall_image_proto{};
      cb.mark_passability_modified(); }
    w.collect(true, true);
    fm_assert(w.at(A) == nullptr);
    fm_assert(w.at(B) == nullptr);

    pool.maybe_mark_stale_all(w.frame_no());
    fm_assert(pool.pooled_count() == before + 2);
}

void test_neighbor_lifecycle(uint32_t div_size)
{
    constexpr chunk_coords_ A{0, 0, 0};
    constexpr chunk_coords_ B{1, 0, 0};
    auto w = world();
    { auto& ca = w[A]; add_ground_all(ca); rebuild_passability(ca); }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);

    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);

    const auto pooled0 = pool.pooled_count();
    clear_ground_all(*w.at(B));
    w.collect(true, true);
    fm_assert(w.at(B) == nullptr);
    tick(w, pool);
    fm_assert(pool.pooled_count() == pooled0 + 1);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);

    const auto pooled1 = pool.pooled_count();
    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);
    fm_assert(pool.pooled_count() == pooled1 - 1);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);
}

void test_self_collect_with_neighbor(uint32_t div_size)
{
    constexpr chunk_coords_ A{0, 0, 0};
    constexpr chunk_coords_ B{1, 0, 0};
    auto w = world();
    { auto& ca = w[A]; add_ground_all(ca); rebuild_passability(ca); }
    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(A));
    (void)count_passable_built(pool, *w.at(B));

    const auto pooled0 = pool.pooled_count();
    clear_ground_all(*w.at(A));
    w.collect(true, true);
    fm_assert(w.at(A) == nullptr);
    fm_assert(w.at(B) != nullptr);

    tick(w, pool);
    fm_assert(pool.pooled_count() == pooled0 + 1);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);
}

void test_pool_reuse_across_different_coord(uint32_t div_size)
{
    constexpr chunk_coords_ X{0, 0, 0};
    constexpr chunk_coords_ Y{3, 3, 0};
    auto w = world();
    { auto& c = w[X]; add_ground_all(c); rebuild_passability(c); }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(X));

    clear_ground_all(*w.at(X));
    w.collect(true, true);
    tick(w, pool);
    const auto pooled_after_collect = pool.pooled_count();
    fm_assert(pooled_after_collect >= 1);

    { auto& c = w[Y]; add_ground_all(c); rebuild_passability(c); }
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(Y));
    fm_assert(pool.pooled_count() == pooled_after_collect - 1);
}

void test_mark_and_ensure_preserve_frame_no(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
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

void test_multiple_pooled_items(uint32_t div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    {
        auto& c = w[COORD];
        tick(w, pool);
        fm_assert(pool.pooled_count() == 0);
        auto grid = pool[c];
        fm_assert(pool.pooled_count() == 0);
    }
    w.collect(true, true); // invalidates c
    tick(w, pool);
    fm_assert(pool.pooled_count() == 1);
    {
        auto& c = w[COORD];
        auto grid = pool[c];
        auto& cʹ = w[COORD_E];
        auto gridʹ = pool[cʹ];
        fm_assert(pool.pooled_count() == 0);
    }
    w.collect(true, true); // invalidates c, cʹ
    tick(w, pool);
    fm_assert(pool.pooled_count() == 2);
    {
        auto& c = w[COORD];
        auto grid = pool[c];
        fm_assert(pool.pooled_count() == 1);
        auto& cʹ = w[COORD_E];
        auto gridʹ = pool[cʹ];
        fm_assert(pool.pooled_count() == 0);
        auto& cʹʹ = w[COORD_Eʹ];
        auto gridʹʹ = pool[cʹʹ];
        fm_assert(pool.pooled_count() == 0);
    }
    w.collect(true, true); // invalidates c, cʹ, cʹʹ
    tick(w, pool);
    fm_assert(pool.pooled_count() == 3);
}

void test_bitview_read_matches_bit(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    add_wall_north(c, {8, 8});
    c[{2, 6}].wall_west() = wall_proto();
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale();

    const auto dc = g.div_count();
    const auto view = g.bits();
    for (uint32_t j = 0; j < dc; j++)
        for (uint32_t i = 0; i < dc; i++)
        {
            const auto idx = Pass::Grid::get_bitmask_index(i, j, dc);
            fm_assert(view.read(idx) == g.bit(idx));
        }
}

void test_partial_collect_then_collect_survivors(uint32_t div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size}};

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
    fm_assert(pool.pooled_count() == 1);

    {
        auto& b = w[COORD_E];
        add_ground_all(b);
        rebuild_passability(b);
        tick(w, pool);
        (void)pool[b];
    }
    fm_assert(pool.pooled_count() == 0);

    {
        clear_ground_all(*w.at(COORD));
        clear_ground_all(*w.at(COORD_Eʹ));
    }
    w.collect(true, true);
    fm_assert(w.at(COORD)    == nullptr);
    fm_assert(w.at(COORD_E)  != nullptr);
    fm_assert(w.at(COORD_Eʹ) == nullptr);

    tick(w, pool);
    fm_assert(pool.pooled_count() == 2);
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
        test_params_preserved_through_pool(ds);
        test_collect_all_chunks_no_crash(ds);
        test_pool_reuse_after_collect(ds);
        test_pool_reuse_neighbors_collected(ds);
        test_neighbor_lifecycle(ds);
        test_self_collect_with_neighbor(ds);
        test_pool_reuse_across_different_coord(ds);
        test_mark_and_ensure_preserve_frame_no(ds);
        test_multiple_pooled_items(ds);
        test_bitview_read_matches_bit(ds);
        test_partial_collect_then_collect_survivors(ds);
    }
    test_pool_destruction_with_live_grids();
}

} // namespace floormat::Test
