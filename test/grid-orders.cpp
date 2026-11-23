#include "app.hpp"
#include "grid-read-only.hpp"
#include "src/grid-registry.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"

namespace floormat::Test {

namespace {

constexpr auto box_params = Pass::Params{16, 32, pass_mode::pass};
constexpr auto fallback_params = Pass::Params{64, 256, pass_mode::pass};
static_assert(fallback_params.bbox_size > Grid::GridRegistry::direct_max_bbox);
// No two of these are neighbors, so marking one's pass grid stale leaves the others alone.
constexpr chunk_coords_ chunk_a{-1, -1, 0}, chunk_b{1, 1, 0}, chunk_c{1, -1, 0};
constexpr chunk_coords_ chunk_without_grid{0, 0, 0};

void test_lookup_sets_flag()
{
    auto w = make_world_with_ground();
    const auto& cw = w;
    Grid::GridRegistry reg;
    auto& pass = reg.pass_pool(box_params);
    auto& objects = reg.object_pool(box_params);
    const auto& cpass = pass;
    const auto& cobjects = objects;
    auto& a = w[chunk_a];
    auto& b = w[chunk_b];
    for (auto* c : { &a, &b })
    {
        pass[*c].build_if_stale();
        objects[*c].build_if_stale();
    }
    const auto free_pass_grids = pass.free_grid_count(), free_object_grids = objects.free_grid_count();

    const auto& without_grid = *cw.at(chunk_without_grid);
    fm_assert(!cpass.find_built_grid(without_grid));
    fm_assert(!cobjects.find_built_grid(without_grid));
    for (auto* c : { &a, &b })
    {
        fm_assert(!pass[*c].is_requested_by_reader());
        fm_assert(!objects[*c].is_requested_by_reader());
    }
    fm_assert_equal(free_pass_grids, pass.free_grid_count());
    fm_assert_equal(free_object_grids, objects.free_grid_count());
    // These would build a grid that a lookup had attached.
    pass.build_if_stale_all();
    objects.build_if_stale_all();
    fm_assert(!cpass.find_built_grid(without_grid));
    fm_assert(!cobjects.find_built_grid(without_grid));

    fm_assert(cpass.find_built_grid(a));
    fm_assert(cobjects.find_built_grid(a));
    fm_assert(pass[a].is_requested_by_reader());
    fm_assert(objects[a].is_requested_by_reader());
    fm_assert(!pass[b].is_requested_by_reader());
    fm_assert(!objects[b].is_requested_by_reader());

    pass[b].mark_stale();
    objects[b].mark_stale();
    fm_assert(!cpass.find_built_grid(b));
    fm_assert(!cobjects.find_built_grid(b));
    fm_assert(pass[b].is_requested_by_reader());
    fm_assert(objects[b].is_requested_by_reader());
}

void test_registry_rebuilds_requested_stale_grids()
{
    auto w = make_world_with_ground();
    Grid::GridRegistry reg;
    auto& pass = reg.pass_pool(box_params);
    auto& objects = reg.object_pool(box_params);
    auto& fallback = reg.pass_pool(fallback_params);
    const auto& cpass = pass;
    const auto& cobjects = objects;
    const auto& cfallback = fallback;
    // a is stale and looked up, b stale and not looked up, c built and looked up.
    auto& a = w[chunk_a];
    auto& b = w[chunk_b];
    auto& c = w[chunk_c];
    for (auto* ch : { &a, &b, &c })
    {
        pass[*ch].build_if_stale();
        objects[*ch].build_if_stale();
        fallback[*ch].build_if_stale();
    }
    const auto a_build_no = pass[a].build_no(), a_fallback_build_no = fallback[a].build_no();
    const auto c_build_no = pass[c].build_no(), c_fallback_build_no = fallback[c].build_no();
    for (auto* ch : { &a, &b })
    {
        pass[*ch].mark_stale();
        objects[*ch].mark_stale();
        fallback[*ch].mark_stale();
    }
    fm_assert(!cpass.find_built_grid(a));
    fm_assert(!cobjects.find_built_grid(a));
    fm_assert(!cfallback.find_built_grid(a));
    fm_assert(cpass.find_built_grid(c));
    fm_assert(cobjects.find_built_grid(c));
    fm_assert(cfallback.find_built_grid(c));

    reg.rebuild_requested_stale_grids();
    fm_assert(!pass[a].is_marked_stale());
    fm_assert(!objects[a].is_marked_stale());
    fm_assert(!fallback[a].is_marked_stale());
    fm_assert(pass[a].build_no() != a_build_no);
    fm_assert(fallback[a].build_no() != a_fallback_build_no);
    fm_assert(pass[b].is_marked_stale());
    fm_assert(objects[b].is_marked_stale());
    fm_assert(fallback[b].is_marked_stale());
    fm_assert_equal(c_build_no, pass[c].build_no());
    fm_assert_equal(c_fallback_build_no, fallback[c].build_no());
    for (auto* ch : { &a, &b, &c })
    {
        fm_assert(!pass[*ch].is_requested_by_reader());
        fm_assert(!objects[*ch].is_requested_by_reader());
        fm_assert(!fallback[*ch].is_requested_by_reader());
    }
    const auto found = cpass.find_built_grid(a);
    fm_assert(found);
    fm_assert_equal(pass[a].build_no(), found.build_no());
    fm_assert(cobjects.find_built_grid(a));
    fm_assert(cfallback.find_built_grid(a));

    // Clears the flags the lookups above set, so a grid then made stale with no lookup stays stale.
    reg.rebuild_requested_stale_grids();
    pass[a].mark_stale();
    objects[a].mark_stale();
    fallback[a].mark_stale();
    reg.rebuild_requested_stale_grids();
    fm_assert(pass[a].is_marked_stale());
    fm_assert(objects[a].is_marked_stale());
    fm_assert(fallback[a].is_marked_stale());
}

} // namespace

void test_grid_orders()
{
    test_lookup_sets_flag();
    test_registry_rebuilds_requested_stale_grids();
}

} // namespace floormat::Test
