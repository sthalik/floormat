#include "app.hpp"
#include "pool-jobs-fixture.hpp"
#include "src/chunk.hpp"
#include "src/grid-order.hpp"
#include "src/grid-registry.hpp"
#include "src/update-world.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/thread-pool.hpp"
#include <array>
#include <type_traits>
#include <utility>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <cr/Pointer.h>

namespace floormat::Test {

namespace {

using grid_kind = grid_order::grid_kind;
using Grid::detail::GridBase;

constexpr auto pass_params = Pass::Params{16, 32, pass_mode::pass};
constexpr auto fallback_params = Pass::Params{64, 256, pass_mode::pass};
constexpr auto object_params = Pass::Params{32, 48, pass_mode::pass};
constexpr auto shoot_through_object_params = Pass::Params{32, 64, pass_mode::shoot_through};
constexpr auto missing_chunk_params = Pass::Params{32, 40, pass_mode::pass};
constexpr u32 cover_div_size = 16;
constexpr auto cover_pass_params = Pass::Params{cover_div_size, pass_mode::shoot_through};
// The params of every pass and object pool that the orders make. The cover pool adds one more pool.
constexpr Pass::Params pass_pool_params[] = {
    pass_params, fallback_params, object_params, shoot_through_object_params, cover_pass_params,
};
constexpr Pass::Params object_pool_params[] = { pass_params, object_params, shoot_through_object_params };
constexpr chunk_coords_ missing_chunk{20, 20, 0};
constexpr chunk_coords_ group_chunks[] = { {1, 1, 0}, {4, 1, 0}, {1, 4, 0}, {4, 4, 0} };
constexpr u32 num_critters_per_group = 5;

// The registry is declared first, so that it outlives the world.
struct world_with_grids
{
    Grid::GridRegistry reg;
    fixture f;
    Array<bptr<critter>> grouped_critters;
    // What GridBase::next_build_no() returned just before and just after the last prepare_world_for_thinking().
    u64 build_no_before_prepare = 0, build_no_after_prepare = 0;
};

struct built_grid_counts
{
    u32 num_grids;
    // Of num_grids, those outside the cover pool's pass pool, which only build_listed_grids() builds.
    u32 num_grids_outside_cover_pass_pool;
};

// Puts num_critters_per_group critters on one tile of each of group_chunks, so that their boxes share cells.
void add_grouped_critters(world_with_grids& s)
{
    arrayReserve(s.grouped_critters, std::size(group_chunks) * num_critters_per_group);
    for (auto ch : group_chunks)
        for (auto i = 0u; i < num_critters_per_group; i++)
            arrayAppend(s.grouped_critters, add_critter(s.f.w, {ch, {7, 7}}));
}

// Moves each grouped critter within its tile, then swaps the positions of two, so that each group keeps its size.
void move_grouped_critters(world_with_grids& s, u32 frame)
{
    const auto& cs = s.grouped_critters;
    for (auto i = 0u; i < (u32)cs.size(); i++)
    {
        auto& C = *cs[i];
        const auto k = (i32)(frame * 3 + i * 7);
        teleport_object(C, C.coord, {(i8)(k % 17 - 8), (i8)(k % 13 - 6)});
    }
    const auto num_groups = (u32)std::size(group_chunks);
    auto& a = *cs[frame % num_groups * num_critters_per_group + frame % num_critters_per_group];
    auto& b = *cs[(frame + 1) % num_groups * num_critters_per_group + (frame + 2) % num_critters_per_group];
    const auto a_coord = a.coord, b_coord = b.coord;
    const auto a_offset = a.offset, b_offset = b.offset;
    teleport_object(a, b_coord, b_offset);
    teleport_object(b, a_coord, a_offset);
}

// Sets is_requested_by_reader on every grid of the pools of pass_params and object_params.
void look_up_grids(const world_with_grids& s)
{
    const auto* pass_pool = s.reg.find_pass_pool(pass_params);
    const auto* object_pool = s.reg.find_object_pool(object_params);
    for (const chunk& c : s.f.w.chunks())
    {
        if (pass_pool)
            (void)pass_pool->find_built_grid(c);
        if (object_pool)
            (void)object_pool->find_built_grid(c);
    }
}

grid_order cover_order(chunk_coords_ ch, u32 octant, u32 update_index)
{
    return {
        .pass_params = cover_pass_params, .cover_div_size = cover_div_size,
        .chunk = ch, .kind = grid_kind::cover, .octant = (u8)octant, .update_index = update_index,
    };
}

bool has_cover_orders(u32 frame)
{
    return frame % 4 != 3;
}

// Two of the orders name the grid of object_params in group_chunks[frame % 4], which look_up_grids() flags.
void add_grid_orders(world_with_grids& s, u32 frame)
{
    auto& orders = s.f.w.grid_orders();
    const auto add = [&](const Pass::Params& p, chunk_coords_ ch, grid_kind kind, u32 update_index) {
        arrayAppend(orders, grid_order{.pass_params = p, .chunk = ch, .kind = kind, .update_index = update_index});
    };
    const auto at = [](u32 x, u32 y) {
        return chunk_coords_{(i16)(x % (u32)num_chunks_xy), (i16)(y % (u32)num_chunks_xy), 0};
    };
    for (auto k = 0u; k < 6; k++)
        add(pass_params, at(frame + k, 2*frame + 3*k), grid_kind::pass, (7*k + frame) % 11);
    add(fallback_params, at(frame, 5*frame + 5), grid_kind::pass, 4);
    add(fallback_params, at(frame + 3, 1), grid_kind::pass, 9);
    for (auto g = 0u; g < (u32)std::size(group_chunks); g++)
        add(object_params, group_chunks[g], grid_kind::object, (5*g + frame) % 11);
    add(object_params, group_chunks[frame % std::size(group_chunks)], grid_kind::object, 10);
    add(shoot_through_object_params, at(frame, frame), grid_kind::object, 6);
    add(shoot_through_object_params, at(5*frame + 5, frame), grid_kind::object, 2);
    add(pass_params, at(frame, frame + 1), grid_kind::object, 7);
    add(missing_chunk_params, missing_chunk, grid_kind::pass, 3);
    add(missing_chunk_params, missing_chunk, grid_kind::object, 5);
    if (has_cover_orders(frame))
    {
        arrayAppend(orders, cover_order(at(2, frame + 3), frame * 5 % 32, 8));
        arrayAppend(orders, cover_order(at(frame + 3, 2), (frame * 11 + 3) % 32, 1));
    }
}

bool is_built_by_last_prepare(const world_with_grids& s, u64 build_no)
{
    return build_no > s.build_no_before_prepare && build_no < s.build_no_after_prepare;
}

void run_frame(world_with_grids& s, u32 frame, const fixture_atlases& a)
{
    auto& w = s.f.w;
    apply_frame(s.f, frame, a);
    move_grouped_critters(s, frame);
    look_up_grids(s);
    add_grid_orders(s, frame);

    Optional<Grid::Object::Grid> grid_named_by_flag_and_two_orders;
    if (!has_cover_orders(frame))
    {
        auto& c = *w.at(group_chunks[frame % std::size(group_chunks)]);
        auto& pool = s.reg.object_pool(object_params);
        fm_assert(pool.has_grid_attached_to(c));
        const auto g = pool[c];
        fm_assert(g.is_stale() && g.is_requested_by_reader());
        grid_named_by_flag_and_two_orders = g;
    }

    s.build_no_before_prepare = GridBase::next_build_no();
    prepare_world_for_thinking(w);
    s.build_no_after_prepare = GridBase::next_build_no();

    fm_assert(w.grid_orders().isEmpty());
    fm_assert(!std::as_const(w).at(missing_chunk));
    fm_assert(!s.reg.find_pass_pool(missing_chunk_params) && !s.reg.find_object_pool(missing_chunk_params));
    if (grid_named_by_flag_and_two_orders)
        fm_assert(is_built_by_last_prepare(s, grid_named_by_flag_and_two_orders->build_no()));
}

template<typename T>
void check_same_elements(ArrayView<const T> a, ArrayView<const T> b)
{
    fm_assert_equal(a.size(), b.size());
    for (auto i = 0u; i < (u32)a.size(); i++)
        fm_assert(a[i] == b[i]);
}

template<typename T>
void check_same_cells(const Grid::Object::Grid& g0, const Grid::Object::Grid& g)
{
    const auto num_cells = g0.div_count() * g0.div_count();
    for (auto i = 0u; i < num_cells; i++)
    {
        const auto v0 = g0.objects_at_cell<T>(i), v = g.objects_at_cell<T>(i);
        fm_assert_equal(v0.size(), v.size());
        for (auto k = 0u; k < v0.size(); k++)
            fm_assert_equal(v0.entry_index(k), v.entry_index(k));
    }
}

// Asserts that the pools of p in s0 and s are both missing, or have grids attached to the same chunks, with the
// same contents and build numbers. Returns how many grids of the pool in s0 its last prepare built.
template<typename Pool>
u32 check_same_grids_in_pools(world_with_grids& s0, world_with_grids& s, const Pass::Params& p)
{
    static constexpr bool is_object_pool = std::is_same_v<Pool, Grid::Object::Pool>;
    const auto has_pool = [&p](const Grid::GridRegistry& reg) {
        if constexpr(is_object_pool)
            return reg.find_object_pool(p) != nullptr;
        else
            return reg.find_pass_pool(p) != nullptr;
    };
    const auto pool_of = [&p](Grid::GridRegistry& reg) -> Pool& {
        if constexpr(is_object_pool)
            return reg.object_pool(p);
        else
            return reg.pass_pool(p);
    };
    fm_assert(has_pool(s0.reg) == has_pool(s.reg));
    if (!has_pool(s0.reg))
        return 0;
    auto &pool0 = pool_of(s0.reg), &pool = pool_of(s.reg);

    u32 num_built = 0;
    for (chunk& c0 : s0.f.w.chunks())
    {
        auto* c = s.f.w.at(c0.coord());
        fm_assert(c);
        const bool has_grid = pool0.has_grid_attached_to(c0);
        fm_assert(has_grid == pool.has_grid_attached_to(*c));
        if (!has_grid)
            continue;
        const auto g0 = pool0[c0], g = pool[*c];
        fm_assert(g0.is_stale() == g.is_stale());
        fm_assert_equal(g0.built_pass_gen(), g.built_pass_gen());
        if (g0.is_stale())
            continue;
        const bool is_built = is_built_by_last_prepare(s0, g0.build_no());
        fm_assert(is_built == is_built_by_last_prepare(s, g.build_no()));
        if (!is_built)
            continue;
        num_built++;
        fm_assert_equal(g0.build_no() - s0.build_no_before_prepare, g.build_no() - s.build_no_before_prepare);
        if constexpr(is_object_pool)
        {
            fm_assert(g0.uses_u32() == g.uses_u32());
            check_same_elements(g0.entry_data(), g.entry_data());
            check_same_elements(g0.entry_chunks(), g.entry_chunks());
            if (g0.uses_u32())
                check_same_cells<u32>(g0, g);
            else
                check_same_cells<u16>(g0, g);
        }
        else
        {
            fm_assert(g0.is_all_empty() == g.is_all_empty());
            check_same_elements(Pass::ConstGrid{g0.raw()}.bits(), Pass::ConstGrid{g.raw()}.bits());
        }
    }
    return num_built;
}

// Asserts that s0 and s have the same pools and grids. Returns the counts of grids that s0's last prepare built.
built_grid_counts check_same_grids(world_with_grids& s0, world_with_grids& s)
{
    fm_assert_equal(s0.f.w.size(), s.f.w.size());
    fm_assert_equal(s0.reg.pool_count(), s.reg.pool_count());
    built_grid_counts ret = {};
    for (const auto& p : pass_pool_params)
    {
        const auto n = check_same_grids_in_pools<Pass::Pool>(s0, s, p);
        ret.num_grids += n;
        ret.num_grids_outside_cover_pass_pool += p == cover_pass_params ? 0 : n;
    }
    for (const auto& p : object_pool_params)
    {
        const auto n = check_same_grids_in_pools<Grid::Object::Pool>(s0, s, p);
        ret.num_grids += n;
        ret.num_grids_outside_cover_pass_pool += n;
    }
    return ret;
}

void run_frames(std::array<ThreadPool*, 5> pools, const fixture_atlases& a)
{
    std::array<Pointer<world_with_grids>, 5> ws;
    for (auto i = 0u; i < ws.size(); i++)
    {
        ws[i] = Pointer<world_with_grids>{InPlaceInit};
        auto& s = *ws[i];
        s.f.w.set_thread_pool(pools[i]);
        s.f.w.set_grid_registry(&s.reg);
        make_fixture(s.f, a);
        add_grouped_critters(s);
    }

    u32 num_frames_listing_2_or_more = 0, num_frames_listing_8_or_more = 0;
    for (auto frame = 0u; frame < num_frames; frame++)
    {
        for (auto& s : ws)
            run_frame(*s, frame, a);
        auto& s0 = *ws[0];
        const auto num_pools = std::size(pass_pool_params) + std::size(object_pool_params) + 1;
        fm_assert_equal((u32)num_pools, s0.reg.pool_count());
        built_grid_counts counts = {};
        for (auto i = 1u; i < ws.size(); i++)
            counts = check_same_grids(s0, *ws[i]);
        num_frames_listing_2_or_more += counts.num_grids_outside_cover_pass_pool >= 2;
        num_frames_listing_8_or_more += counts.num_grids_outside_cover_pass_pool >= 8;
        // Without cover orders, only build_listed_grids() takes build numbers, one per listed grid.
        if (!has_cover_orders(frame))
            for (const auto& s : ws)
                fm_assert_equal((u64)counts.num_grids, s->build_no_after_prepare - s->build_no_before_prepare - 1);
    }
    fm_assert(num_frames_listing_2_or_more >= 10);
    fm_assert(num_frames_listing_8_or_more >= 5);
}

} // namespace

void test_grid_build_jobs()
{
    ThreadPool pool2{2}, pool4{4}, pool8{8}, pool4_without_caller{4, false};
    const fixture_atlases a;
    run_frames({ nullptr, &pool2, &pool4, &pool8, &pool4_without_caller }, a);
}

} // namespace floormat::Test
