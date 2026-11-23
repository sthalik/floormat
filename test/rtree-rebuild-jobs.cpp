#include "app.hpp"
#include "pool-jobs-fixture.hpp"
#include "src/chunk.hpp"
#include "src/collision.hpp"
#include "src/old-rtree.hpp"
#include "src/old-rtree-search.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/thread-pool.hpp"
#include <array>
#include <bit>
#include <cr/GrowableArray.h>

namespace floormat::Test {

namespace {

using RTree = chunk::RTree;

constexpr u32 num_repetitions = 2;

struct rtree_entry
{
    object_id id;
    std::array<float, 4> rect;

    bool operator==(const rtree_entry&) const = default;
};

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
