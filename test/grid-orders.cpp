#include "app.hpp"
#include "grid-read-only.hpp"
#include "run.hpp"
#include "src/grid-order.hpp"
#include "src/grid-registry.hpp"
#include "src/update-world.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/spatial-index.hpp"
#include "src/point.inl"
#include "loader/loader.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/contains.hpp"
#include "compat/nanosecond.inl"
#include <initializer_list>
#include <cr/GrowableArray.h>

namespace floormat::Test {

namespace {

constexpr auto box_params = Pass::Params{16, 32, pass_mode::pass};
constexpr auto fallback_params = Pass::Params{64, 256, pass_mode::pass};
static_assert(fallback_params.bbox_size > Grid::GridRegistry::direct_max_bbox);
// No two of these are neighbors, so marking one's pass grid stale leaves the others alone.
constexpr chunk_coords_ chunk_a{-1, -1, 0}, chunk_b{1, 1, 0}, chunk_c{1, -1, 0};
constexpr chunk_coords_ chunk_without_grid{0, 0, 0};
constexpr chunk_coords_ chunks_3x3[] = {
    {-1, -1, 0}, { 0, -1, 0}, { 1, -1, 0},
    {-1,  0, 0}, { 0,  0, 0}, { 1,  0, 0},
    {-1,  1, 0}, { 0,  1, 0}, { 1,  1, 0},
};
constexpr u32 cover_div_size = 16;

grid_order cover_order(chunk_coords_ ch, u32 octant, u32 update_index)
{
    return {
        .pass_params = Pass::Params{cover_div_size, pass_mode::shoot_through}, .cover_div_size = cover_div_size,
        .chunk = ch, .kind = grid_order::grid_kind::cover, .octant = (u8)octant, .update_index = update_index,
    };
}

wall_image_proto make_wall()
{
    return { loader.wall_atlas("empty"), 0 };
}

void assert_no_chunk_passability_modified(const world& w)
{
    for (auto ch : chunks_3x3)
        fm_assert(!w.at(ch)->is_passability_modified());
}

void add_wall_in_each_chunk(world& w)
{
    const auto wall = make_wall();
    for (auto ch : chunks_3x3)
    {
        auto& c = w[ch];
        c[local_coords{(u8)(5 + ch.x), (u8)(9 - ch.y)}].wall_north() = wall;
        c.mark_walls_modified();
    }
}

struct rtree_entry
{
    object_id id;
    chunk::RTree::Rect rect;

    bool operator==(const rtree_entry&) const = default;
};

void append_rtree_entries(const chunk::RTree& t, Array<rtree_entry>& out)
{
    const float lo[2] = { -2*chunk_size<float>, -2*chunk_size<float> };
    const float hi[2] = { 2*chunk_size<float>, 2*chunk_size<float> };
    t.Search(lo, hi, [&](object_id id, const chunk::RTree::Rect& r) {
        arrayAppend(out, rtree_entry{id, r});
        return true;
    });
}

void test_world_ensure_passability()
{
    constexpr chunk_coords_ wall_chunks[] = { {-1, 0, 0}, {1, 1, 0}, {0, -1, 0} };
    constexpr chunk_coords_ scenery_chunks[] = { {0, 0, 0}, {1, -1, 0} };
    const auto wall = make_wall();
    const auto add_walls_and_scenery = [&](world& w) {
        w[wall_chunks[0]][local_coords{4, 9}].wall_north() = wall;
        w[wall_chunks[1]][local_coords{11, 2}].wall_west() = wall;
        w[wall_chunks[2]][local_coords{7, 13}].wall_north() = wall;
        for (auto ch : wall_chunks)
            w[ch].mark_walls_modified();
        add_scenery(w, {scenery_chunks[0], {3, 11}}, {5, -7}, {29, 19}, pass_mode::blocked);
        add_scenery(w, {scenery_chunks[1], {15, 6}}, {23, 0}, {37, 13}, pass_mode::shoot_through);
    };
    auto w = make_world_with_ground();
    auto reference_world = make_world_with_ground();
    (void)add_colliders(w);
    (void)add_colliders(reference_world);
    add_walls_and_scenery(w);
    add_walls_and_scenery(reference_world);
    for (auto ch : wall_chunks)
        fm_assert(w.at(ch)->is_passability_modified());
    for (auto ch : scenery_chunks)
        fm_assert(w.at(ch)->is_passability_modified());

    w.ensure_passability();
    assert_no_chunk_passability_modified(w);
    for (auto& c : reference_world.chunks())
    {
        (void)c.rtree();
        (void)c.hole_rtree();
    }

    Array<rtree_entry> found, expected;
    arrayReserve(found, 16);
    arrayReserve(expected, 16);
    const auto& cw = w;
    const auto& creference_world = reference_world;
    for (auto ch : chunks_3x3)
    {
        arrayClear(found);
        arrayClear(expected);
        append_rtree_entries(*cw.at(ch)->rtree(), found);
        append_rtree_entries(*cw.at(ch)->hole_rtree(), found);
        append_rtree_entries(*creference_world.at(ch)->rtree(), expected);
        append_rtree_entries(*creference_world.at(ch)->hole_rtree(), expected);
        fm_assert_equal(expected.size(), found.size());
        for (const auto& e : expected)
            fm_assert(contains(found, e));
    }
}

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

void test_moved_critter_object_grid_found_built()
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    auto w = make_world_with_ground();
    Grid::GridRegistry reg;
    w.set_grid_registry(&reg);
    const auto& cw = w;
    const auto& creg = reg;
    const auto npc = add_critter_at(w, {3*64 + 7, 5*64 + 11}, 24, pass_mode::blocked);
    u32 num_frames_with_empty_handle = 0;
    for (u32 frame = 0; frame < 40; frame++)
    {
        auto i = npc->index();
        npc->teleport_to(i, npc->position() + Vector2i{1, 0}, rotation_COUNT);
        prepare_world_for_thinking(w);
        const auto* pool = creg.find_object_pool(box_params);
        if (!pool || !pool->find_built_grid(*cw.at(ch)))
        {
            // As raycast-loop.cpp does for an empty handle.
            num_frames_with_empty_handle++;
            arrayAppend(w.grid_orders(), grid_order{
                .pass_params = box_params, .chunk = ch, .kind = grid_order::grid_kind::object, .update_index = 0,
            });
        }
    }
    // Frame 0 has no grid attached for its lookup to flag.
    fm_assert_equal(1u, num_frames_with_empty_handle);
}

void test_run_grid_orders_per_kind()
{
    constexpr auto pass_params = Pass::Params{32, 32, pass_mode::pass};
    constexpr auto object_params = Pass::Params{16, 24, pass_mode::pass};
    constexpr auto missing_chunk_params = Pass::Params{32, 48, pass_mode::pass};
    constexpr auto ch = chunk_coords_{0, 0, 0}, missing = chunk_coords_{5, -4, 0};
    constexpr u32 octant = 9;
    auto w = make_world_with_ground();
    w.ensure_passability();
    Grid::GridRegistry reg;
    w.set_grid_registry(&reg);
    const auto& cw = w;
    const auto& creg = reg;
    fm_assert(!creg.find_pass_pool(pass_params));
    fm_assert(!creg.find_object_pool(object_params));
    fm_assert(!creg.find_cover_pool(Grid::Cover::Params{cover_div_size}));
    fm_assert(!cw.at(missing));

    auto& orders = w.grid_orders();
    arrayAppend(orders, grid_order{
        .pass_params = pass_params, .chunk = ch, .kind = grid_order::grid_kind::pass,
    });
    arrayAppend(orders, grid_order{
        .pass_params = object_params, .chunk = ch, .kind = grid_order::grid_kind::object,
    });
    arrayAppend(orders, cover_order(ch, octant, 0));
    arrayAppend(orders, grid_order{
        .pass_params = missing_chunk_params, .chunk = missing, .kind = grid_order::grid_kind::pass,
    });
    arrayAppend(orders, grid_order{
        .pass_params = missing_chunk_params, .chunk = missing, .kind = grid_order::grid_kind::object,
    });
    run_grid_orders(w);

    fm_assert(w.grid_orders().isEmpty());
    const auto& c = *cw.at(ch);
    const auto* pass_pool = creg.find_pass_pool(pass_params);
    fm_assert(pass_pool && pass_pool->find_built_grid(c));
    const auto* object_pool = creg.find_object_pool(object_params);
    fm_assert(object_pool && object_pool->find_built_grid(c));
    fm_assert(object_pool->pass_pool().find_built_grid(c));
    const auto* cover_pool = creg.find_cover_pool(Grid::Cover::Params{cover_div_size});
    fm_assert(cover_pool);
    const auto g = cover_pool->find_built_grid(c);
    fm_assert(g && g.built_octants() & 1u << octant);
    fm_assert(!cw.at(missing));
    fm_assert(!creg.find_pass_pool(missing_chunk_params));
    fm_assert(!creg.find_object_pool(missing_chunk_params));
}

Grid::Cover::Grid make_built_cover_grid(Grid::Cover::Pool& pool, chunk& c)
{
    auto g = pool[c];
    g.build_if_stale();
    fm_assert_equal(1u, g.built_octants());
    return g;
}

void test_run_grid_orders_octant_fill_cap()
{
    constexpr auto max_fills = Grid::Cover::max_octant_fills_per_frame;
    static_assert(max_fills == 2, "the expected octant sets below assume this cap");
    constexpr auto bit = [](u32 k) { return 1u << k; };
    auto w = make_world_with_ground();
    w.ensure_passability();
    Grid::GridRegistry reg;
    w.set_grid_registry(&reg);
    auto& pool = reg.cover_pool(Grid::Cover::Params{cover_div_size});
    auto& orders = w.grid_orders();

    {
        constexpr auto ch = chunk_coords_{-1, -1, 0};
        auto g = make_built_cover_grid(pool, w[ch]);
        const auto fills = pool.octant_fills();
        for (u32 k : { 3u, 7u, 11u, 19u, 27u })
            arrayAppend(orders, cover_order(ch, k, 0));
        run_grid_orders(w);
        fm_assert_equal(max_fills, pool.octant_fills() - fills);
        fm_assert_equal(bit(0) | bit(3) | bit(7), g.built_octants());
    }
    {
        constexpr auto ch = chunk_coords_{0, -1, 0};
        auto g = make_built_cover_grid(pool, w[ch]);
        const auto fills = pool.octant_fills();
        for (u32 k : { 1u, 2u, 3u })
            arrayAppend(orders, cover_order(ch, k, 0));
        for (u32 k : { 4u, 5u, 6u })
            arrayAppend(orders, cover_order(ch, k, 1));
        run_grid_orders(w);
        fm_assert_equal(2*max_fills, pool.octant_fills() - fills);
        fm_assert_equal(bit(0) | bit(1) | bit(2) | bit(4) | bit(5), g.built_octants());
    }
    {
        // Sorted, index 3 fills 8 and 9, then index 7 fills 10 and 12. Unsorted, index 7 would fill 9 and 10,
        // then index 3 only 8.
        constexpr auto ch = chunk_coords_{1, -1, 0};
        auto g = make_built_cover_grid(pool, w[ch]);
        const auto fills = pool.octant_fills();
        for (u32 k : { 9u, 10u, 12u })
            arrayAppend(orders, cover_order(ch, k, 7));
        for (u32 k : { 8u, 9u, 10u })
            arrayAppend(orders, cover_order(ch, k, 3));
        run_grid_orders(w);
        fm_assert_equal(bit(0) | bit(8) | bit(9) | bit(10) | bit(12), g.built_octants());
        fm_assert_equal(2*max_fills, pool.octant_fills() - fills);
    }
    {
        constexpr auto ch = chunk_coords_{-1, 0, 0};
        auto g = make_built_cover_grid(pool, w[ch]);
        g.mark_stale();
        const auto fills = pool.octant_fills();
        arrayAppend(orders, cover_order(ch, 5, 0));
        run_grid_orders(w);
        fm_assert_equal(2u, pool.octant_fills() - fills);
        fm_assert(!g.is_marked_stale());
        fm_assert_equal(bit(0) | bit(5), g.built_octants());
    }
    {
        // The second order for octant 13 counts nothing, so 14 still fits under the cap.
        constexpr auto ch = chunk_coords_{0, 0, 0};
        auto g = make_built_cover_grid(pool, w[ch]);
        const auto fills = pool.octant_fills();
        for (u32 k : { 13u, 13u, 14u })
            arrayAppend(orders, cover_order(ch, k, 0));
        run_grid_orders(w);
        fm_assert_equal(2u, pool.octant_fills() - fills);
        fm_assert_equal(bit(0) | bit(13) | bit(14), g.built_octants());
    }
    {
        // As in Cover::Point::finder::find(), the rebuild that reaches the cap leaves the octant unfilled.
        constexpr auto ch_built = chunk_coords_{1, 0, 0}, ch_stale = chunk_coords_{-1, 1, 0};
        auto built = make_built_cover_grid(pool, w[ch_built]);
        auto stale = make_built_cover_grid(pool, w[ch_stale]);
        stale.mark_stale();
        const auto fills = pool.octant_fills();
        arrayAppend(orders, cover_order(ch_built, 15, 0));
        arrayAppend(orders, cover_order(ch_stale, 17, 0));
        run_grid_orders(w);
        fm_assert_equal(max_fills, pool.octant_fills() - fills);
        fm_assert_equal(bit(0) | bit(15), built.built_octants());
        fm_assert(!stale.is_marked_stale());
        fm_assert_equal(bit(0), stale.built_octants());
    }
    fm_assert(orders.isEmpty());
}

bptr<critter> make_critter(world& w, point pos)
{
    auto proto = Run::make_proto(1.f);
    proto.playable = false;
    proto.offset = pos.offset();
    return w.make_object<critter>(w.make_id(), pos.coord(), move(proto));
}

void test_orders_run_before_critters_think()
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto pass_params = Pass::Params{32, 32, pass_mode::pass};
    constexpr u32 octant = 21;
    for (bool is_direct_update : { false, true })
    {
        auto w = make_world_with_ground();
        Grid::GridRegistry reg;
        w.set_grid_registry(&reg);
        const auto& cw = w;
        const auto& creg = reg;
        const auto npc = make_critter(w, point{ch, {3, 5}, {}});
        add_wall_in_each_chunk(w);
        for (auto c : chunks_3x3)
            fm_assert(cw.at(c)->is_passability_modified());
        arrayAppend(w.grid_orders(), grid_order{
            .pass_params = pass_params, .chunk = ch, .kind = grid_order::grid_kind::pass, .update_index = 0,
        });
        arrayAppend(w.grid_orders(), cover_order(ch, octant, 1));
        w.init_scripts();
        if (is_direct_update)
        {
            auto i = npc->index();
            npc->update(npc, i, 17*Millisecond);
        }
        else
            update_world(w, chunks_3x3, 17*Millisecond);

        assert_no_chunk_passability_modified(w);
        fm_assert(w.grid_orders().isEmpty());
        const auto& c = *cw.at(ch);
        const auto* pass_pool = creg.find_pass_pool(pass_params);
        fm_assert(pass_pool && pass_pool->find_built_grid(c));
        const auto* cover_pool = creg.find_cover_pool(Grid::Cover::Params{cover_div_size});
        fm_assert(cover_pool);
        const auto g = cover_pool->find_built_grid(c);
        fm_assert(g && g.built_octants() & 1u << octant);
        w.finish_scripts();
    }
}

void test_prepare_world_without_registry()
{
    auto w = make_world_with_ground();
    fm_assert(!w.grid_registry());
    add_wall_in_each_chunk(w);
    for (auto ch : chunks_3x3)
        fm_assert(w.at(ch)->is_passability_modified());
    prepare_world_for_thinking(w);
    assert_no_chunk_passability_modified(w);
}

} // namespace

void test_grid_orders()
{
    test_world_ensure_passability();
    test_lookup_sets_flag();
    test_registry_rebuilds_requested_stale_grids();
    test_moved_critter_object_grid_found_built();
    test_run_grid_orders_per_kind();
    test_run_grid_orders_octant_fill_cap();
    test_orders_run_before_critters_think();
    test_prepare_world_without_registry();
}

} // namespace floormat::Test
