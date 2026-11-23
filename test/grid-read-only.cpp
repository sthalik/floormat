#include "app.hpp"
#include "grid-read-only.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-object.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-registry.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/hole.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/anim-atlas.hpp"
#include "src/search.hpp"
#include "src/intra-coord.inl"
#include "src/point.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/array-size.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function-view.hpp"
#include <utility>
#include <cr/GrowableArray.h>

namespace floormat::Test {

namespace {

constexpr chunk_coords_ COORD{0, 0, 0};
constexpr chunk_coords_ COORD_E{1, 0, 0};

void add_ground_all(chunk& c)
{
    const auto floor = tile_image_proto{ loader.ground_atlas("tiles"), 0 };
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = floor;
}

} // namespace

void rebuild_passability(chunk& c)
{
    c.mark_passability_modified();
    c.ensure_passability();
}

world make_world_with_ground()
{
    auto w = world();
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
        {
            auto& c = w[chunk_coords_{x, y, 0}];
            add_ground_all(c);
            rebuild_passability(c);
        }
    return w;
}

bptr<critter> add_critter_at(world& w, Vector2i px, u8 size, pass_mode pass)
{
    const auto pt = point::normalize_coords(point{COORD, {}, {}}, px);
    critter_proto p;
    p.name = "npc"_s;
    p.bbox_size = Vector2ub{size};
    p.offset = pt.offset();
    p.pass = pass;
    return w.make_object<critter>(w.make_id(), pt.coord(), move(p));
}

namespace {

void add_scenery(world& w, global_coords pos, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass)
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_offset = bbox_offset;
    p.bbox_size   = bbox_size;
    p.pass        = pass;
    (void)w.make_scenery(w.make_id(), pos, move(p));
}

void add_hole(world& w, global_coords pos, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass)
{
    hole_proto p;
    p.bbox_offset = bbox_offset;
    p.bbox_size   = bbox_size;
    p.pass        = pass;
    (void)w.make_object<hole>(w.make_id(), pos, p);
}

template<typename Pool>
void put_grid_on_free_list(Pool& pool)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);
    (void)pool[c];
}

template<typename Pool>
void check_lookup_attaches_nothing(Pool& pool)
{
    put_grid_on_free_list(pool);
    fm_assert_equal(1u, pool.free_grid_count());
    auto w = make_world_with_ground();
    const auto& c = *w.at(COORD);
    fm_assert(!std::as_const(pool).find_built_grid(c));
    fm_assert_equal(1u, pool.free_grid_count());
    // Attaching would have taken the free grid, and build_if_stale_all() would have built it.
    pool.build_if_stale_all();
    fm_assert(!std::as_const(pool).find_built_grid(c));
    fm_assert_equal(1u, pool.free_grid_count());
}

void test_find_built_grid_writes_nothing()
{
    {
        Pass::Pool pool{Pass::Params{16, pass_mode::pass}};
        check_lookup_attaches_nothing(pool);
    }
    {
        Pass::Pool pass_pool{Pass::Params{16, pass_mode::pass}};
        Grid::Object::Pool pool{pass_pool};
        check_lookup_attaches_nothing(pool);
    }
    {
        Pass::Pool pass_pool{Pass::Params{16, pass_mode::shoot_through}};
        Grid::Cover::Pool pool{Grid::Cover::Params{16}, pass_pool};
        check_lookup_attaches_nothing(pool);
    }
}

void check_same_grid(const Pass::ConstGrid& found, const Pass::Grid& g)
{
    fm_assert(found);
    const auto dc = g.div_count();
    fm_assert_equal(dc, found.div_count());
    fm_assert_equal(g.built_pass_gen(), found.built_pass_gen());
    fm_assert_equal(g.build_no(), found.build_no());
    fm_assert_equal(g.is_all_empty(), found.is_all_empty());
    const auto bits = found.bits();
    fm_assert_equal((usize)Grid::detail::bitmask_size(dc) / (sizeof(usize)*8), bits.size());
    for (auto k = 0u; k < bits.size(); k++)
        fm_assert_equal(g.bits().data[k], bits[k]);
    for (auto i = 0u; i < dc*dc; i++)
        fm_assert_equal(g.bit(i), found.bit(i));
    const auto pos = intra_coord{Vector2i{617, 389}};
    fm_assert_equal(g.get_bitmask_index_from_coord(pos), found.get_bitmask_index_from_coord(pos));
}

void test_pass_grid_lookup()
{
    auto w = make_world_with_ground();
    auto& c = w[COORD];
    auto& e = w[COORD_E];
    Pass::Pool pool{Pass::Params{16, 32, pass_mode::pass}};
    const auto& cpool = pool;
    auto g = pool[c], ge = pool[e];
    g.build_if_stale();
    ge.build_if_stale();
    check_same_grid(cpool.find_built_grid(c), g);
    check_same_grid(cpool.find_built_grid(e), ge);

    c[local_coords{14, 9}].wall_north() = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    rebuild_passability(c);
    fm_assert(!cpool.find_built_grid(c));
    fm_assert(!cpool.find_built_grid(e));

    g.build_if_stale();
    check_same_grid(cpool.find_built_grid(c), g);
    fm_assert(!g.is_all_empty());
    fm_assert(!cpool.find_built_grid(e));
    ge.build_if_stale();
    check_same_grid(cpool.find_built_grid(e), ge);
}

void check_same_grid(const Grid::Object::ConstGrid& found, const Grid::Object::Grid& g, Array<bool>& entry_blocked)
{
    fm_assert(found);
    const auto dc = g.div_count();
    fm_assert_equal(dc, found.div_count());
    fm_assert_equal(g.built_pass_gen(), found.built_pass_gen());
    fm_assert(!found.uses_u32());
    fm_assert_equal(g.entry_data().data(), found.entry_data().data());
    fm_assert_equal(g.entry_data().size(), found.entry_data().size());
    fm_assert_equal(g.entry_chunks().size(), found.entry_chunks().size());
    fm_assert_equal(g.blocked_objects(Search::never_continue), found.blocked_objects(Search::never_continue));
    (void)found.blocked_objects(Search::never_continue, entry_blocked);
    for (auto i = 0u; i < dc*dc; i++)
    {
        fm_assert_equal(g.objects_at_cell<u16>(i).size(), found.objects_at_cell<u16>(i).size());
        fm_assert_equal(g.is_blocked<u16>(i, entry_blocked), found.is_blocked<u16>(i, entry_blocked));
        fm_assert_equal(g.is_empty_at<u16>(i, 1), found.is_empty_at<u16>(i, 1));
    }
}

void test_object_grid_lookup()
{
    auto w = make_world_with_ground();
    auto& c = w[COORD];
    const auto npc = add_critter_at(w, {9*64, 6*64}, 24, pass_mode::blocked);
    const auto npc_e = add_critter_at(w, {1024 + 3*64, 11*64}, 24, pass_mode::blocked);
    Pass::Pool pass_pool{Pass::Params{16, 32, pass_mode::pass}};
    Grid::Object::Pool pool{pass_pool};
    const auto& cpool = pool;
    const auto& cpass_pool = pass_pool;
    auto g = pool[c];
    auto pass = pass_pool[c];
    g.build_if_stale();
    pass.build_if_stale();
    Array<bool> entry_blocked;
    check_same_grid(cpool.find_built_grid(c), g, entry_blocked);
    fm_assert_equal(1uz, cpool.find_built_grid(c).entry_data().size());
    fm_assert(cpass_pool.find_built_grid(c));

    auto i = npc->index();
    npc->teleport_to(i, {COORD, local_coords{10, 6}}, npc->offset, npc->r);
    fm_assert(!cpool.find_built_grid(c));
    fm_assert(cpass_pool.find_built_grid(c));
    g.build_if_stale();
    check_same_grid(cpool.find_built_grid(c), g, entry_blocked);

    i = npc_e->index();
    npc_e->teleport_to(i, {COORD_E, local_coords{3, 12}}, npc_e->offset, npc_e->r);
    fm_assert(!cpool.find_built_grid(c));
    fm_assert(cpass_pool.find_built_grid(c));
    g.build_if_stale();
    check_same_grid(cpool.find_built_grid(c), g, entry_blocked);
}

void test_cover_grid_lookup()
{
    constexpr u32 filled_octant = 5, unfilled_octant = 19;
    auto w = make_world_with_ground();
    auto& c = w[COORD];
    for (u8 i = 3; i < 13; i++)
        c[local_coords{i, 7}].wall_north() = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    rebuild_passability(c);
    Pass::Pool pass_pool{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool pool{Grid::Cover::Params{16}, pass_pool};
    const auto& cpool = pool;
    fm_assert(!cpool.find_built_grid(c));

    auto g = pool[c];
    (void)g.ensure_octant(filled_octant);
    const auto found = cpool.find_built_grid(c);
    fm_assert(found);
    fm_assert_equal(g.built_octants(), found.built_octants());
    fm_assert(found.built_octants() & 1u << filled_octant);
    fm_assert(!(found.built_octants() & 1u << unfilled_octant));
    fm_assert_equal(g.div_count(), found.div_count());
    fm_assert_equal(g.build_no(), found.build_no());
    fm_assert_equal(g.built_pass_gen(), found.built_pass_gen());
    const auto pos = intra_coord{Vector2i{617, 389}};
    fm_assert_equal(g.get_cell_index_from_coord(pos), found.get_cell_index_from_coord(pos));
    const auto dc = g.div_count();
    u32 num_blocked = 0;
    for (auto i = 0u; i < dc*dc; i++)
    {
        fm_assert_equal(g.distance(i, filled_octant), found.distance(i, filled_octant));
        num_blocked += found.distance(i, filled_octant) == Grid::Cover::blocked;
    }
    fm_assert(num_blocked > 0);
}

void test_registry_lookup()
{
    constexpr Pass::Params pass_params[] = {
        {16, 32, pass_mode::pass},      // the direct table
        {16, 300, pass_mode::pass},     // the fallback list: bbox_size above direct_max_bbox
    };
    // Cover::Params::validate() takes only divisors of chunk_size_xy, which are powers of two, so every cover pool
    // is in the direct table.
    constexpr auto cover_params = Grid::Cover::Params{32};
    constexpr auto cover_pass_params = Pass::Params{cover_params.div_size, pass_mode::shoot_through};

    Grid::GridRegistry reg;
    const auto& creg = reg;
    for (const auto& p : pass_params)
    {
        fm_assert(!creg.find_object_pool(p));
        fm_assert(!creg.find_pass_pool(p));
    }
    fm_assert(!creg.find_cover_pool(cover_params));
    fm_assert(!creg.find_pass_pool(cover_pass_params));

    for (const auto& p : pass_params)
    {
        auto& pass = reg.pass_pool(p);
        fm_assert(creg.find_pass_pool(p) == &pass);
        fm_assert(!creg.find_object_pool(p));
        auto& objects = reg.object_pool(p);
        fm_assert(creg.find_object_pool(p) == &objects);
        fm_assert(creg.find_pass_pool(p) == &pass);
        fm_assert(&objects.pass_pool() == &pass);
    }
    fm_assert(creg.find_pass_pool(pass_params[0]) != creg.find_pass_pool(pass_params[1]));
    fm_assert(!creg.find_cover_pool(cover_params));
    fm_assert(!creg.find_pass_pool(cover_pass_params));
    auto& cover = reg.cover_pool(cover_params);
    fm_assert(creg.find_cover_pool(cover_params) == &cover);
    fm_assert(creg.find_pass_pool(cover_pass_params) == &reg.pass_pool(cover_pass_params));
}

} // namespace

pred_object_ids add_colliders(world& w)
{
    const auto wall = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    constexpr auto W = chunk_coords_{-1, 0, 0}, S = chunk_coords_{0, 1, 0};
    constexpr auto SE = chunk_coords_{1, 1, 0}, NW = chunk_coords_{-1, -1, 0};
    auto& c = w[COORD];
    for (u8 i = 3; i < 13; i++)
        c[local_coords{i, 7}].wall_north() = wall;
    for (u8 j = 2; j < 10; j++)
        c[local_coords{5, j}].wall_west() = wall;
    c[local_coords{0, 4}].wall_west() = wall;
    c[local_coords{6, 0}].wall_north() = wall;
    w[COORD_E][local_coords{0, 3}].wall_west() = wall;
    w[COORD_E][local_coords{15, 10}].wall_north() = wall;
    w[SE][local_coords{15, 15}].wall_north() = wall;
    w[SE][local_coords{15, 15}].wall_west() = wall;
    w[NW][local_coords{0, 0}].wall_north() = wall;
    w[NW][local_coords{0, 0}].wall_west() = wall;
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
        {
            auto& ch = w[chunk_coords_{x, y, 0}];
            const auto k = (u8)(3*(y+1) + x+1);
            for (u8 i = 0; i < 6; i++)
            {
                ch[local_coords{(u8)(1 + k + i), (u8)(2 + k)}].wall_north() = wall;
                ch[local_coords{(u8)(14 - k), (u8)(5 + i)}].wall_west() = wall;
            }
        }

    // Odd sizes and offsets put rect edges at odd positions, where boxes of an odd bbox_size end.
    add_scenery(w, {COORD, {9, 10}}, {}, {40, 24}, pass_mode::blocked);
    add_scenery(w, {COORD, {11, 3}}, {}, {21, 37}, pass_mode::see_through);
    add_scenery(w, {COORD, {2, 12}}, {3, -5}, {30, 31}, pass_mode::shoot_through);
    add_scenery(w, {COORD, {13, 13}}, {}, {32, 32}, pass_mode::pass);
    add_scenery(w, {COORD, {6, 13}}, {}, {19, 19}, pass_mode::blocked);
    add_scenery(w, {COORD, {12, 1}}, {}, {17, 17}, pass_mode::see_through);
    add_scenery(w, {COORD_E, {15, 5}}, {20, 0}, {40, 16}, pass_mode::blocked);
    add_scenery(w, {COORD_E, {15, 13}}, {31, 7}, {27, 45}, pass_mode::see_through);
    add_scenery(w, {chunk_coords_{1, -1, 0}, {15, 15}}, {25, 25}, {33, 33}, pass_mode::shoot_through);
    add_scenery(w, {S, {0, 0}}, {-19, -21}, {25, 23}, pass_mode::see_through);
    add_scenery(w, {W, {15, 8}}, {30, 0}, {24, 40}, pass_mode::shoot_through);
    add_scenery(w, {SE, {15, 0}}, {20, -20}, {40, 40}, pass_mode::blocked);

    // A gap in the north wall row, the blocked scenery's east part made shoot_through, and a gap in the wall on
    // COORD's east border, cut by a hole in COORD_E.
    add_hole(w, {COORD, {7, 7}}, {0, -32}, {48, 24}, pass_mode::pass);
    add_hole(w, {COORD, {9, 10}}, {12, 0}, {16, 40}, pass_mode::shoot_through);
    add_hole(w, {COORD_E, {0, 3}}, {-32, 0}, {24, 48}, pass_mode::pass);

    auto door = loader.scenery("door1");
    door.pass = pass_mode::blocked;
    door.frame = (u16)(door.atlas->info().nframes - 1);
    const auto door_id = w.make_scenery(w.make_id(), {COORD, {14, 12}}, move(door))->id;

    (void)add_critter_at(w, {8*64, 5*64}, 24, pass_mode::blocked);
    (void)add_critter_at(w, {990, 301}, 21, pass_mode::blocked);
    (void)add_critter_at(w, {-30, -29}, 17, pass_mode::blocked);
    (void)add_critter_at(w, {1024 + 989, 600}, 31, pass_mode::blocked);
    (void)add_critter_at(w, {1024 + 985, 1024 + 400}, 29, pass_mode::blocked);
    (void)add_critter_at(w, {501, -399}, 27, pass_mode::shoot_through);
    (void)add_critter_at(w, {-700, 1500}, 23, pass_mode::see_through);
    const auto critter_id = add_critter_at(w, {992, 992}, 24, pass_mode::blocked)->id;

    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
            rebuild_passability(w[chunk_coords_{x, y, 0}]);
    return { door_id, critter_id };
}

namespace {

intra_coord node_of_cell(const Pass::Params& params, u32 i, u32 j)
{
    const auto d = params.div_size(), o = params.node_offset();
    return intra_coord{Vector2i{(i32)(i*d + o), (i32)(j*d + o)}};
}

// Appends, for each node, is_passable_by_rtree() with a null pred, then with each of preds.
void append_rtree_results(const world& w, chunk_coords_ ch, const Pass::Params& params,
                          ArrayView<const Search::pred> preds, Array<bool>& results)
{
    const auto dc = params.div_count;
    arrayReserve(results, results.size() + (usize)dc*dc * (1 + preds.size()));
    for (auto j = 0u; j < dc; j++)
        for (auto i = 0u; i < dc; i++)
        {
            const auto pt = point{ch, node_of_cell(params, i, j)};
            arrayAppend(results, Search::is_passable_by_rtree(w, params, pt, nullptr));
            for (const auto& p : preds)
                arrayAppend(results, Search::is_passable_by_rtree(w, params, pt, &p));
        }
}

// Appends, for each node, the pass grid's bit, then the bit masked by the object grid for each of preds.
void append_grid_results(chunk& c, Pass::Pool& pass_pool, Grid::Object::Pool& object_pool,
                         ArrayView<const Search::pred> preds, Array<bool>& results)
{
    pass_pool[c].build_if_stale();
    object_pool[c].build_if_stale();
    const auto pass = std::as_const(pass_pool).find_built_grid(c);
    const auto objects = std::as_const(object_pool).find_built_grid(c);
    fm_assert(pass && objects);
    const auto bits = pass.bits();
    auto masked_bits = Array<usize>{NoInit, bits.size() * preds.size()};
    Array<bool> entry_blocked;
    for (auto k = 0u; k < preds.size(); k++)
    {
        (void)objects.blocked_objects(preds[k], entry_blocked);
        objects.mask_pass_bits(bits, masked_bits.sliceSize(k * bits.size(), bits.size()), entry_blocked);
    }
    const auto& params = pass_pool.params();
    const auto dc = params.div_count;
    arrayReserve(results, results.size() + (usize)dc*dc * (1 + preds.size()));
    for (auto j = 0u; j < dc; j++)
        for (auto i = 0u; i < dc; i++)
        {
            const auto index = pass.get_bitmask_index_from_coord(node_of_cell(params, i, j));
            const auto [word, mask] = Grid::detail::GridBase::word_and_mask(index);
            arrayAppend(results, pass.bit(index));
            for (auto k = 0u; k < preds.size(); k++)
                arrayAppend(results, (masked_bits[k * bits.size() + word] & mask) != 0);
        }
}

// Adds the clear grid bits to num_clear_bits[0] for the null pred and to num_clear_bits[1 + k] for preds[k].
void assert_results_equal(ArrayView<const bool> rtree_results, ArrayView<const bool> grid_results,
                          const Pass::Params& params, chunk_coords_ ch, ArrayView<u32> num_clear_bits)
{
    fm_assert_equal(grid_results.size(), rtree_results.size());
    const auto num_results_per_node = (u32)num_clear_bits.size(), dc = params.div_count;
    for (auto n = 0u; n < (u32)grid_results.size(); n++)
    {
        const auto k = n % num_results_per_node, node = n / num_results_per_node;
        if (grid_results[n] != rtree_results[n]) [[unlikely]]
            fm_abort("div_size %u bbox_size %u min_pass %u: chunk (%d, %d) node (%u, %u) pred %u: "
                     "grid %d, rtree %d",
                     params.div_size(), params.bbox_size, (unsigned)params.min_pass, (int)ch.x, (int)ch.y,
                     node % dc, node / dc, k, (int)grid_results[n], (int)rtree_results[n]);
        num_clear_bits[k] += !grid_results[n];
    }
}

void test_is_passable_by_rtree()
{
    constexpr auto COORD_MISSING = chunk_coords_{2, 0, 0};
    auto w = make_world_with_ground();
    const auto [door_id, critter_id] = add_colliders(w);
    const auto blocks_all_but_door = Search::blocks_all_but(door_id);
    const auto blocks_all_but_critter = Search::blocks_all_but(critter_id);
    const Search::pred preds[] = { blocks_all_but_door, Search::without_critters, blocks_all_but_critter };
    constexpr u32 num_results_per_node = 1 + (u32)array_size(preds);

    Array<Pass::Params> cases;
    arrayReserve(cases, 24);
    for (u32 d : { 8u, 16u, 32u })
        for (u32 bbox_size : { d, d + 7, 3*d, 2*d + 1 })
            for (auto min_pass : { pass_mode::pass, pass_mode::shoot_through })
                arrayAppend(cases, Pass::Params{d, bbox_size, min_pass}.validate());

    // COORD_MISSING's results, recorded while it doesn't exist and compared with its grids once it does.
    auto missing_chunk_results = Array<Array<bool>>{ValueInit, cases.size()};
    fm_assert(!std::as_const(w).at(COORD_MISSING));
    for (auto n = 0u; n < (u32)cases.size(); n++)
        append_rtree_results(w, COORD_MISSING, cases[n], preds, missing_chunk_results[n]);

    Array<bool> rtree_results, grid_results;
    for (const auto& params : cases)
    {
        Pass::Pool pass_pool{params};
        Grid::Object::Pool object_pool{pass_pool};
        u32 num_clear_bits[num_results_per_node] = {};
        u32 num_nodes_compared = 0;
        for (i16 y = -1; y <= 1; y++)
            for (i16 x = -1; x <= 1; x++)
            {
                const auto ch = chunk_coords_{x, y, 0};
                arrayClear(rtree_results);
                arrayClear(grid_results);
                append_rtree_results(w, ch, params, preds, rtree_results);
                append_grid_results(w[ch], pass_pool, object_pool, preds, grid_results);
                assert_results_equal(rtree_results, grid_results, params, ch, num_clear_bits);
                num_nodes_compared += (u32)grid_results.size() / num_results_per_node;
            }
        for (auto k = 0u; k < num_results_per_node; k++)
        {
            fm_assert(num_clear_bits[k] * 200 >= num_nodes_compared);
            fm_assert((num_nodes_compared - num_clear_bits[k]) * 2 >= num_nodes_compared);
        }
        // without_critters lets every critter through, blocks_all_but_critter one and blocks_all_but_door none.
        fm_assert_equal(num_clear_bits[0], num_clear_bits[2]);
        fm_assert(num_clear_bits[1] > num_clear_bits[3] && num_clear_bits[3] > num_clear_bits[0]);
    }

    auto& missing = w[COORD_MISSING];
    rebuild_passability(missing);
    for (auto n = 0u; n < (u32)cases.size(); n++)
    {
        const auto& params = cases[n];
        Pass::Pool pass_pool{params};
        Grid::Object::Pool object_pool{pass_pool};
        u32 num_clear_bits[num_results_per_node] = {}, num_clear_bits_with_chunk[num_results_per_node] = {};
        arrayClear(rtree_results);
        arrayClear(grid_results);
        append_grid_results(missing, pass_pool, object_pool, preds, grid_results);
        assert_results_equal(missing_chunk_results[n], grid_results, params, COORD_MISSING, num_clear_bits);
        append_rtree_results(w, COORD_MISSING, params, preds, rtree_results);
        assert_results_equal(rtree_results, grid_results, params, COORD_MISSING, num_clear_bits_with_chunk);
        fm_assert(num_clear_bits[0] > 0 && num_clear_bits[1] > num_clear_bits[0]);
    }
}

} // namespace

void test_grid_read_only()
{
    test_find_built_grid_writes_nothing();
    test_pass_grid_lookup();
    test_object_grid_lookup();
    test_cover_grid_lookup();
    test_registry_lookup();
    test_is_passable_by_rtree();
}

} // namespace floormat::Test
