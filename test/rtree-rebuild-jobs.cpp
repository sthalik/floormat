#include "app.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/hole.hpp"
#include "src/collision.hpp"
#include "src/tile-image.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/old-rtree.hpp"
#include "src/old-rtree-search.hpp"
#include "loader/loader.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/thread-pool.hpp"
#include <array>
#include <bit>
#include <cr/GrowableArray.h>

namespace floormat::Test {

namespace {

using RTree = chunk::RTree;

constexpr i16 num_chunks_xy = 6;
constexpr u32 num_frames = 24, num_repetitions = 2;
constexpr chunk_coords_ new_chunk_coord{num_chunks_xy, 2, 0};

struct fixture_atlases
{
    bptr<wall_atlas> wall1 = loader.wall_atlas("empty"), wall2 = loader.wall_atlas("concrete1");
    bptr<ground_atlas> floor = loader.ground_atlas("tiles"), blocker = loader.ground_atlas("texel");
    scenery_proto table = loader.scenery("table1"), door = loader.scenery("door1");
};

struct fixture
{
    world w;
    Array<bptr<scenery>> doors, tables;
    Array<bptr<critter>> critters;
    Array<bptr<hole>> holes;
};

struct rtree_entry
{
    object_id id;
    std::array<float, 4> rect;

    bool operator==(const rtree_entry&) const = default;
};

bptr<critter> add_critter(world& w, global_coords pos)
{
    critter_proto p;
    p.name = "npc"_s;
    p.bbox_size = Vector2ub{24};
    return w.make_object<critter>(w.make_id(), pos, move(p));
}

bptr<hole> add_hole(world& w, global_coords pos, Vector2b offset, Vector2ub size, pass_mode pass)
{
    hole_proto p;
    p.offset = offset;
    p.bbox_size = size;
    p.pass = pass;
    return w.make_object<hole>(w.make_id(), pos, p);
}

// The holes reach into the east and south neighbours, cutting their border walls at {0, 8} and {8, 0}.
void make_fixture_chunk(fixture& f, chunk_coords_ ch, const fixture_atlases& a)
{
    auto& w = f.w;
    auto& c = w[ch];
    const auto k = (u32)(ch.x * num_chunks_xy + ch.y);
    for (auto i = 0u; i < TILE_COUNT; i++)
        c[i].ground() = { a.floor, 0 };
    c[local_coords{(u8)(2 + k % 7), 3}].ground() = { a.blocker, 0 };
    c[local_coords{0, 8}].wall_west() = { a.wall2, 0 };
    c[local_coords{8, 0}].wall_north() = { a.wall1, 0 };
    for (u8 x = 4; x < 8; x++)
        c[local_coords{x, 6}].wall_north() = { (x + k) % 2 ? a.wall1 : a.wall2, 0 };
    c[local_coords{4, 6}].wall_west() = { a.wall1, 0 };
    c.mark_modified();

    arrayAppend(f.tables, w.make_scenery(w.make_id(), {ch, {10, 10}}, scenery_proto(a.table)));
    if (k % 3 == 0)
        arrayAppend(f.doors, w.make_scenery(w.make_id(), {ch, {12, 4}}, scenery_proto(a.door)));
    arrayAppend(f.critters, add_critter(w, {ch, {3, 12}}));
    arrayAppend(f.critters, add_critter(w, {ch, {9, 13}}));
    const auto pass = k % 2 ? pass_mode::pass : pass_mode::see_through;
    arrayAppend(f.holes, add_hole(w, {ch, {15, 8}}, {31, 0}, {64, 40}, pass));
    arrayAppend(f.holes, add_hole(w, {ch, {8, 15}}, {0, 31}, {40, 64}, pass));
}

void make_fixture(fixture& f, const fixture_atlases& a)
{
    for (i16 y = 0; y < num_chunks_xy; y++)
        for (i16 x = 0; x < num_chunks_xy; x++)
            make_fixture_chunk(f, {x, y, 0}, a);
}

void set_walls(world& w, chunk_coords_ ch, u8 y, const bptr<wall_atlas>& atlas)
{
    auto& c = w[ch];
    for (u8 x = 2; x < 10; x++)
        c[local_coords{x, y}].wall_north() = { atlas, 0 };
    c.mark_walls_modified();
}

void set_door_pass(fixture& f, pass_mode pass)
{
    for (const auto& d : f.doors)
        d->set_bbox(d->offset, d->bbox_offset, d->bbox_size, pass);
}

void teleport_object(object& e, global_coords pos, Vector2b offset)
{
    auto i = e.index();
    e.teleport_to(i, pos, offset, rotation_COUNT);
}

void mark_every_chunk_modified(world& w)
{
    for (auto& c : w.chunks())
        c.mark_passability_modified();
}

void apply_frame(fixture& f, u32 frame, const fixture_atlases& a)
{
    auto& w = f.w;
    const auto& C = f.critters[frame % f.critters.size()];
    teleport_object(*C, C->coord, {(i8)(frame % 13), (i8)(frame % 7)});

    switch (frame)
    {
    case 2: set_walls(w, {1, 1, 0}, 9, a.wall2); break;
    case 3:
        set_walls(w, {1, 1, 0}, 9, {});
        set_walls(w, {4, 2, 0}, 11, a.wall1);
        set_walls(w, {2, 4, 0}, 11, a.wall2);
        break;
    case 4:
        for (const auto ch : { chunk_coords_{0, 0, 0}, chunk_coords_{5, 5, 0}, chunk_coords_{3, 3, 0} })
            arrayAppend(f.tables, w.make_scenery(w.make_id(), {ch, {6, 9}}, scenery_proto(a.table)));
        break;
    case 5:
        for (auto i : { f.tables.size() - 1, (usize)14 })
        {
            auto& s = *f.tables[i];
            s.chunk().remove_object(s, s.index());
        }
        break;
    case 6: set_door_pass(f, pass_mode::pass); break;
    case 7: teleport_object(*f.holes[2*14], {{2, 2, 0}, {15, 4}}, {31, 0}); break;
    case 8:
        set_door_pass(f, pass_mode::blocked);
        teleport_object(*f.holes[2*21 + 1], {{3, 3, 0}, {0, 0}}, {-32, -32});
        break;
    case 9: make_fixture_chunk(f, new_chunk_coord, a); break;
    case 10: case 16: mark_every_chunk_modified(w); break;
    case 11:
        for (i16 x = 0; x < num_chunks_xy; x++)
        {
            auto& c = w[chunk_coords_{x, (i16)(x % 3), 0}];
            c[local_coords{(u8)(x + 5), 14}].ground() = { a.blocker, 0 };
            c.mark_ground_modified();
        }
        break;
    case 12:
        for (i16 y = 0; y < num_chunks_xy; y++)
        {
            auto& c = w[chunk_coords_{3, y, 0}];
            c[local_coords{0, 8}].wall_west() = {};
            c.mark_walls_modified();
        }
        break;
    case 13:
    {
        auto& h = *f.holes[2*(1*num_chunks_xy + 4)];
        h.chunk().remove_object(h, h.index());
        break;
    }
    case 15:
        arrayAppend(f.holes, add_hole(w, {{2, 2, 0}, {15, 12}}, {20, 0}, {80, 24}, pass_mode::shoot_through));
        break;
    case 17: set_door_pass(f, pass_mode::see_through); break;
    case 18:
        for (auto i = 0u; i < 6; i++)
            teleport_object(*f.critters[i*5], {{(i16)(5 - i), (i16)i, 0}, {7, 7}}, {});
        break;
    case 19:
        for (auto& c : w.chunks())
        {
            c[local_coords{1, 1}].wall_north() = { a.wall1, 0 };
            c.mark_walls_modified();
        }
        break;
    case 21: set_walls(w, {0, 5, 0}, 2, a.wall1); break;
    case 22:
        set_walls(w, {5, 0, 0}, 2, a.wall2);
        set_walls(w, new_chunk_coord, 2, a.wall2);
        break;
    default: break;
    }
}

Array<rtree_entry> entries_of(const RTree& t)
{
    Array<rtree_entry> ret;
    arrayReserve(ret, (usize)t.Count());
    const auto lo = Vector2{-2*(float)chunk_size_xy}, hi = Vector2{3*(float)chunk_size_xy};
    t.Search(lo.data(), hi.data(), [&](object_id id, const RTree::Rect& r) {
        arrayAppend(ret, rtree_entry{id, {r.m_min[0], r.m_min[1], r.m_max[0], r.m_max[1]}});
        return true;
    });
    fm_assert_equal((usize)t.Count(), ret.size());
    return ret;
}

void check_same_entries(const RTree& t0, const RTree& t)
{
    const auto e0 = entries_of(t0), e = entries_of(t);
    fm_assert_equal(e0.size(), e.size());
    for (auto i = 0u; i < e0.size(); i++)
        fm_assert(e0[i] == e[i]);
}

void check_same_rtrees(const world& w0, const world& w)
{
    fm_assert_equal(w0.size(), w.size());
    for (const chunk& c0 : w0.chunks())
    {
        const chunk* c = w.at(c0.coord());
        fm_assert(c);
        check_same_entries(*c0.rtree(), *c->rtree());
        check_same_entries(*c0.hole_rtree(), *c->hole_rtree());
    }
}

// Asserts that the fixture's holes on chunk borders cut a neighbour's wall. In chunk {1, 1}, only the
// see-through hole of chunk {1, 0} can leave a see-through piece.
void check_border_holes(const world& w)
{
    const auto& c = *w.at(chunk_coords_{1, 1, 0});
    fm_assert_equal(4, c.hole_rtree()->Count());
    bool has_see_through_piece = false;
    for (const auto& e : entries_of(*c.rtree()))
    {
        const auto x = std::bit_cast<collision_data>(e.id);
        has_see_through_piece |= x.type == (u64)collision_type::geometry && x.pass == (u64)pass_mode::see_through;
    }
    fm_assert(has_see_through_piece);
}

u32 num_modified_chunks(const world& w)
{
    u32 ret = 0;
    for (const chunk& c : w.chunks())
        ret += c.is_passability_modified();
    return ret;
}

// Returns how many frames rebuilt two or more chunks, which runs the rebuilds as pool jobs.
u32 run_frames(std::array<ThreadPool*, 5> pools, const fixture_atlases& a)
{
    std::array<fixture, 5> fs;
    for (auto i = 0u; i < fs.size(); i++)
    {
        fs[i].w.set_thread_pool(pools[i]);
        make_fixture(fs[i], a);
    }
    u32 ret = 0;
    for (auto frame = 0u; frame < num_frames; frame++)
    {
        for (auto& f : fs)
        {
            apply_frame(f, frame, a);
            const auto num_wall1_refs = a.wall1.use_count(), num_wall2_refs = a.wall2.use_count();
            if (&f == &fs[0])
                ret += num_modified_chunks(f.w) >= 2;
            f.w.ensure_passability();
            fm_assert_equal(num_wall1_refs, a.wall1.use_count());
            fm_assert_equal(num_wall2_refs, a.wall2.use_count());
            fm_assert(num_modified_chunks(f.w) == 0);
        }
        if (frame == 0)
            check_border_holes(fs[0].w);
        for (auto i = 1u; i < fs.size(); i++)
            check_same_rtrees(fs[0].w, fs[i].w);
    }
    return ret;
}

} // namespace

void test_rtree_rebuild_jobs()
{
    ThreadPool pool2{2}, pool4{4}, pool8{8}, pool4_without_caller{4, false};
    const auto pools = std::array<ThreadPool*, 5>{ nullptr, &pool2, &pool4, &pool8, &pool4_without_caller };
    const fixture_atlases a;
    const auto num_nodes = RTree::node_pool.alive_count(), num_list_nodes = RTree::list_node_pool.alive_count();
    for (auto i = 0u; i < num_repetitions; i++)
    {
        fm_assert(run_frames(pools, a) >= 10);
        fm_assert_equal(num_nodes, RTree::node_pool.alive_count());
        fm_assert_equal(num_list_nodes, RTree::list_node_pool.alive_count());
    }
}

} // namespace floormat::Test
