#include "app.hpp"
#include "pool-jobs-fixture.hpp"
#include "src/chunk.hpp"
#include "src/grid-order.hpp"
#include "src/grid-registry.hpp"
#include "src/update-world.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/thread-pool.hpp"
#include <array>
#include <bit>
#include <cr/GrowableArray.h>
#include <cr/Pointer.h>

namespace floormat::Test {

namespace {

using grid_kind = grid_order::grid_kind;
using Grid::detail::GridBase;
using Grid::Cover::octant_count;

// A narrow grid holds its distances as u8, which needs a div_count() below UINT8_MAX.
constexpr u32 narrow_div_size = 16, non_narrow_div_size = 4;
static_assert(chunk_size_xy / narrow_div_size < UINT8_MAX && chunk_size_xy / non_narrow_div_size >= UINT8_MAX);
constexpr Grid::Cover::Params cover_pool_params[] = {
    Grid::Cover::Params{narrow_div_size}, Grid::Cover::Params{non_narrow_div_size},
};
constexpr auto pass_params = Pass::Params{16, 32, pass_mode::pass};
constexpr auto object_params = Pass::Params{32, 48, pass_mode::pass};
constexpr auto narrow_cover_pass_params = Pass::Params{narrow_div_size, pass_mode::shoot_through};
constexpr chunk_coords_ missing_chunk{20, 20, 0};
constexpr chunk_coords_ last_chunk{num_chunks_xy - 1, num_chunks_xy - 1, 0};
// A non-narrow grid takes megabytes, so its orders name only these chunks.
constexpr chunk_coords_ non_narrow_chunks[] = { {2, 2, 0}, {3, 2, 0} };
constexpr u32 num_worlds = 6, reference_world = num_worlds - 1;

// The registry is declared first, so that it outlives the world.
struct world_with_grids
{
    Grid::GridRegistry reg;
    fixture f;
    // What GridBase::next_build_no() returned just before and just after the frame's grid orders ran.
    u64 build_no_before_prepare = 0, build_no_after_prepare = 0;
};

// One octant that the reference world filled in the current frame, a rebuild's octant 0 included.
struct filled_octant
{
    u32 div_size;
    chunk_coords_ chunk;
    u32 octant;
};

struct reference_counts
{
    u32 num_orders_dropped_by_cap = 0, num_rebuilds = 0, num_rebuilds_of_built_grids = 0;
    u32 num_grids_marked_stale = 0, num_octant_0_orders_for_stale_grids = 0, num_orders_for_built_octants = 0;
    u32 num_frames_filling_4_or_more_grids = 0;
    // Bit k is set once the narrow pool filled octant k.
    u32 narrow_octants_filled = 0;
    Array<filled_octant> octants_filled_in_frame;
    Array<Grid::detail::CoverGrid*> grids_filled_in_frame;
};

chunk_coords_ at(u32 x, u32 y)
{
    return {(i16)(x % (u32)num_chunks_xy), (i16)(y % (u32)num_chunks_xy), 0};
}

// The chunk whose narrow grid the frame's orders name under several update_index values.
chunk_coords_ own_chunk(u32 frame)
{
    return at(frame, frame);
}

void add_grid_orders(world& w, u32 frame)
{
    auto& orders = w.grid_orders();
    const auto add = [&](const Pass::Params& p, chunk_coords_ ch, grid_kind kind, u32 update_index) {
        arrayAppend(orders, grid_order{.pass_params = p, .chunk = ch, .kind = kind, .update_index = update_index});
    };
    const auto add_cover = [&](u32 div_size, chunk_coords_ ch, u32 octant, u32 update_index) {
        arrayAppend(orders, grid_order{
            .pass_params = Pass::Params{div_size, pass_mode::shoot_through}, .cover_div_size = div_size,
            .chunk = ch, .kind = grid_kind::cover, .octant = (u8)(octant % octant_count),
            .update_index = update_index,
        });
    };
    const auto own = own_chunk(frame);

    for (auto k = 0u; k < 4; k++)
        add(pass_params, at(frame + k, 2*frame + 3*k), grid_kind::pass, (5*k + frame) % 16);
    add(object_params, at(frame, frame + 1), grid_kind::object, 2);
    add(object_params, at(2*frame, 5), grid_kind::object, 9);
    // The pass grids of own and of its east neighbor, which the cover orders for own read.
    add(narrow_cover_pass_params, own, grid_kind::pass, 0);
    add(narrow_cover_pass_params, at(frame + 1, frame), grid_kind::pass, 6);

    // Four orders under update_index 0 to 2, so that two share one.
    for (auto j = 0u; j < 4; j++)
        add_cover(narrow_div_size, at(frame + 2*j + 1, 3*frame + j), 7*frame + 5*j, (j + frame) % 3);
    // Octant 0 of the previous frame's own_chunk(), which some frames mark stale, then another grid under the same
    // update_index.
    add_cover(narrow_div_size, own_chunk(frame + num_chunks_xy - 1), 0, 3);
    add_cover(narrow_div_size, at(frame + 3, frame + 1), 3*frame + 17, 3);
    // Three grids under one update_index, so that the cap drops one at least.
    for (auto j = 0u; j < 3; j++)
        add_cover(narrow_div_size, at(frame + 2*j, 4 + j), 11*frame + 9*j + 1, 4);
    for (auto j = 0u; j < 3; j++)
        add_cover(narrow_div_size, own, 3*frame + 8*j, 5 + j);
    // One octant twice, then another one, under one update_index.
    const auto twice = at(frame + 2, 2*frame);
    add_cover(narrow_div_size, twice, 13*frame, 8);
    add_cover(narrow_div_size, twice, 13*frame, 8);
    add_cover(narrow_div_size, twice, 13*frame + 6, 8);
    // Octants that read chunks past the world's west and east edges.
    add_cover(narrow_div_size, at(0, frame), 16 + frame % 8, 10);
    add_cover(narrow_div_size, last_chunk, frame % 8, 11);
    add_cover(narrow_div_size, missing_chunk, frame, 12);
    // apply_frame() makes new_chunk_coord in frame 9.
    add_cover(narrow_div_size, new_chunk_coord, 2*frame + 1, 13);
    if (frame % 4 == 1)
        add_cover(non_narrow_div_size, non_narrow_chunks[frame / 4 % 2], 5*frame, 14);
}

// Insertion sort, as run_grid_orders() sorts.
void sort_by_update_index(ArrayView<grid_order> orders)
{
    for (u32 i = 1; i < (u32)orders.size(); i++)
    {
        const auto o = orders[i];
        auto j = i;
        for (; j > 0 && orders[j-1].update_index > o.update_index; j--)
            orders[j] = orders[j-1];
        orders[j] = o;
    }
}

void add_filled_octant(reference_counts& counts, Grid::Cover::Grid g, const grid_order& o, u32 octant)
{
    arrayAppend(counts.octants_filled_in_frame, filled_octant{o.cover_div_size, o.chunk, octant});
    bool is_grid_listed = false;
    for (auto* x : counts.grids_filled_in_frame)
        is_grid_listed |= x == g.raw();
    if (!is_grid_listed)
        arrayAppend(counts.grids_filled_in_frame, g.raw());
    if (o.cover_div_size == narrow_div_size)
        counts.narrow_octants_filled |= 1u << octant;
}

// Takes the cover orders out of w.grid_orders(), runs the others through prepare_world_for_thinking(), then runs
// the cover orders by the serial loop that build_grids_and_run_cover_orders() had before the pool jobs.
void prepare_with_serial_cover_orders(world_with_grids& s, reference_counts& counts)
{
    auto& w = s.f.w;
    auto& reg = s.reg;
    auto& all_orders = w.grid_orders();
    Array<grid_order> orders;
    arrayReserve(orders, all_orders.size());
    u32 num_other_orders = 0;
    for (const auto& o : all_orders)
        if (o.kind == grid_kind::cover)
            arrayAppend(orders, o);
        else
            all_orders[num_other_orders++] = o;
    arrayRemoveSuffix(all_orders, all_orders.size() - num_other_orders);
    prepare_world_for_thinking(w);
    sort_by_update_index(orders);

    arrayClear(counts.octants_filled_in_frame);
    arrayClear(counts.grids_filled_in_frame);
    u32 num_octant_fills = 0;
    // As in Cover::Point::finder::find(), the cap is tested before a rebuild and again before an octant fill.
    const auto is_octant_fill_cap_reached = [&] {
        return num_octant_fills >= Grid::Cover::max_octant_fills_per_frame;
    };
    for (u32 i = 0; i < (u32)orders.size(); i++)
    {
        const auto& o = orders[i];
        if (i > 0 && o.update_index != orders[i-1].update_index)
            num_octant_fills = 0;
        auto* c = w.at(o.chunk);
        if (!c || o.kind != grid_order::grid_kind::cover || is_octant_fill_cap_reached())
        {
            counts.num_orders_dropped_by_cap += c && is_octant_fill_cap_reached();
            continue;
        }
        auto g = reg.cover_pool(Grid::Cover::Params{o.cover_div_size})[*c];
        if (g.is_marked_stale())
        {
            counts.num_rebuilds_of_built_grids += g.build_no() != 0;
            g.build_if_stale();
            num_octant_fills++;
            counts.num_rebuilds++;
            counts.num_octant_0_orders_for_stale_grids += o.octant == 0;
            add_filled_octant(counts, g, o, 0);
        }
        else if (g.built_octants() & 1u << o.octant)
            counts.num_orders_for_built_octants++;
        if (!is_octant_fill_cap_reached() && !(g.built_octants() & 1u << o.octant))
        {
            num_octant_fills += g.ensure_octant(o.octant);
            add_filled_octant(counts, g, o, o.octant);
        }
        else if (!(g.built_octants() & 1u << o.octant))
            counts.num_orders_dropped_by_cap++;
    }
    counts.num_frames_filling_4_or_more_grids += counts.grids_filled_in_frame.size() >= 4;
}

bool is_numbered_by_last_prepare(const world_with_grids& s, u64 build_no)
{
    return build_no > s.build_no_before_prepare && build_no < s.build_no_after_prepare;
}

void check_same_distances(const Grid::Cover::Grid& g0, const Grid::Cover::Grid& g, u32 octants)
{
    const auto num_cells = g0.div_count() * g0.div_count();
    fm_assert_equal(num_cells, g.div_count() * g.div_count());
    for (auto bits = octants; bits; bits &= bits - 1)
    {
        const auto k = (u32)std::countr_zero(bits);
        for (auto i = 0u; i < num_cells; i++)
            fm_assert(g0.distance(i, k) == g.distance(i, k));
    }
}

// Asserts that the reference world s0 and s have the same cover pools, grids, octant fill counts, and distances in
// the octants s0 filled in the frame, or in every built octant when needs_every_built_octant_compared.
void check_same_cover_grids(world_with_grids& s0, world_with_grids& s, const reference_counts& counts,
                            bool needs_every_built_octant_compared)
{
    fm_assert_equal(s0.f.w.size(), s.f.w.size());
    fm_assert_equal(s0.reg.pool_count(), s.reg.pool_count());
    for (const auto& p : cover_pool_params)
    {
        const bool has_pool = s0.reg.find_cover_pool(p) != nullptr;
        fm_assert(has_pool == (s.reg.find_cover_pool(p) != nullptr));
        if (!has_pool)
            continue;
        auto &pool0 = s0.reg.cover_pool(p), &pool = s.reg.cover_pool(p);
        fm_assert_equal(pool0.octant_fills(), pool.octant_fills());
        for (chunk& c0 : s0.f.w.chunks())
        {
            auto* c = s.f.w.at(c0.coord());
            fm_assert(c);
            const bool has_grid = pool0.has_grid_attached_to(c0);
            fm_assert(has_grid == pool.has_grid_attached_to(*c));
            if (!has_grid)
                continue;
            const auto g0 = pool0[c0], g = pool[*c];
            fm_assert(g0.is_marked_stale() == g.is_marked_stale());
            fm_assert_equal(g0.built_pass_gen(), g.built_pass_gen());
            fm_assert_equal(g0.built_octants(), g.built_octants());
            const bool is_numbered = is_numbered_by_last_prepare(s0, g0.build_no());
            fm_assert(is_numbered == is_numbered_by_last_prepare(s, g.build_no()));
            if (needs_every_built_octant_compared)
                check_same_distances(g0, g, g0.built_octants());
        }
    }
    for (const auto& x : counts.octants_filled_in_frame)
    {
        const auto p = Grid::Cover::Params{x.div_size};
        check_same_distances(s0.reg.cover_pool(p)[*s0.f.w.at(x.chunk)], s.reg.cover_pool(p)[*s.f.w.at(x.chunk)],
                             1u << x.octant);
    }
}

// Asserts that s0 and s gave their rebuilt cover grids the same build numbers, counted from next_build_no() just
// before their last prepare_world_for_thinking(). Returns how many grids s0 rebuilt.
u32 check_same_cover_build_numbers(world_with_grids& s0, world_with_grids& s)
{
    u32 num_rebuilt = 0;
    for (const auto& p : cover_pool_params)
    {
        if (!s0.reg.find_cover_pool(p))
            continue;
        auto &pool0 = s0.reg.cover_pool(p), &pool = s.reg.cover_pool(p);
        for (chunk& c0 : s0.f.w.chunks())
        {
            if (!pool0.has_grid_attached_to(c0))
                continue;
            const auto g0 = pool0[c0], g = pool[*s.f.w.at(c0.coord())];
            if (!is_numbered_by_last_prepare(s0, g0.build_no()))
                continue;
            num_rebuilt++;
            fm_assert_equal(g0.build_no() - s0.build_no_before_prepare, g.build_no() - s.build_no_before_prepare);
        }
    }
    return num_rebuilt;
}

// In frames 6, 14 and 22, before apply_frame(), marks the previous frame's own_chunk() narrow grid stale on every
// world. That frame's orders built it.
void mark_previous_own_grid_stale(std::array<Pointer<world_with_grids>, num_worlds>& ws, u32 frame,
                                  reference_counts& counts)
{
    if (frame % 8 == 6)
    {
        counts.num_grids_marked_stale++;
        for (auto& s : ws)
        {
            auto& pool = s->reg.cover_pool(cover_pool_params[0]);
            auto& c = *s->f.w.at(own_chunk(frame - 1));
            fm_assert(pool.has_grid_attached_to(c));
            auto g = pool[c];
            fm_assert(!g.is_marked_stale());
            g.mark_stale();
        }
    }
}

void run_frames(std::array<ThreadPool*, num_worlds> pools, const fixture_atlases& a)
{
    std::array<Pointer<world_with_grids>, num_worlds> ws;
    for (auto i = 0u; i < ws.size(); i++)
    {
        ws[i] = Pointer<world_with_grids>{InPlaceInit};
        auto& s = *ws[i];
        s.f.w.set_thread_pool(pools[i]);
        s.f.w.set_grid_registry(&s.reg);
        make_fixture(s.f, a);
    }

    reference_counts counts;
    arrayReserve(counts.octants_filled_in_frame, 64);
    arrayReserve(counts.grids_filled_in_frame, 64);
    u32 num_rebuilt_grids_numbered = 0;
    for (auto frame = 0u; frame < num_frames; frame++)
    {
        mark_previous_own_grid_stale(ws, frame, counts);
        for (auto& s : ws)
            apply_frame(s->f, frame, a);
        for (auto i = 0u; i < ws.size(); i++)
        {
            auto& s = *ws[i];
            add_grid_orders(s.f.w, frame);
            s.build_no_before_prepare = GridBase::next_build_no();
            if (i == reference_world)
                prepare_with_serial_cover_orders(s, counts);
            else
                prepare_world_for_thinking(s.f.w);
            s.build_no_after_prepare = GridBase::next_build_no();
            fm_assert(s.f.w.grid_orders().isEmpty());
        }
        // In clang-asan, comparing every built octant in every frame took the test to 1.1 s.
        for (auto i = 0u; i < reference_world; i++)
            check_same_cover_grids(*ws[reference_world], *ws[i], counts, frame % 4 == 3);
        for (auto i = 1u; i < reference_world; i++)
            num_rebuilt_grids_numbered += check_same_cover_build_numbers(*ws[0], *ws[i]);
    }

    fm_assert(counts.num_orders_dropped_by_cap >= 10);
    fm_assert(counts.num_rebuilds >= 5);
    fm_assert(counts.num_rebuilds_of_built_grids > counts.num_grids_marked_stale);
    fm_assert(counts.num_frames_filling_4_or_more_grids >= 5);
    fm_assert_equal(~0u, counts.narrow_octants_filled);
    fm_assert(counts.num_grids_marked_stale >= 1);
    fm_assert(counts.num_octant_0_orders_for_stale_grids >= 1 && counts.num_orders_for_built_octants >= 1);
    fm_assert(num_rebuilt_grids_numbered >= 5);
    const auto* non_narrow_pool = ws[reference_world]->reg.find_cover_pool(cover_pool_params[1]);
    fm_assert(non_narrow_pool && non_narrow_pool->octant_fills() >= 2);
}

} // namespace

void test_cover_octant_fill_jobs()
{
    ThreadPool pool2{2}, pool4{4}, pool8{8}, pool4_without_caller{4, false};
    const fixture_atlases a;
    run_frames({ nullptr, &pool2, &pool4, &pool8, &pool4_without_caller, nullptr }, a);
}

} // namespace floormat::Test
