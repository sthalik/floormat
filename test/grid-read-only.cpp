#include "app.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-object.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-registry.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/intra-coord.inl"
#include "src/point.inl"
#include "loader/loader.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function-view.hpp"
#include <utility>

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

// px is relative to the center of COORD's first tile.
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

void test_grid_read_only()
{
    test_find_built_grid_writes_nothing();
    test_pass_grid_lookup();
    test_object_grid_lookup();
    test_cover_grid_lookup();
    test_registry_lookup();
}

} // namespace floormat::Test
