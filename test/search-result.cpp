#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/thread-pool.hpp"
#include "src/search-result.hpp"
#include "src/search-node.hpp"
#include <algorithm>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>

namespace floormat {

namespace { struct Test_PathPool {}; }

using psrpa = path_search_result_pool_access<Test_PathPool>;
using node = path_search_result::node;

template<>
struct path_search_result_pool_access<Test_PathPool> final
{
    static const Pointer<node>& get_node(const path_search_result& p) { return p._node; }
    static const auto& get_pool() { return path_search_result::_pool; }
    static usize pool_size();
    static void clear_pool() { path_search_result::_pool = {}; }
};

usize path_search_result_pool_access<Test_PathPool>::pool_size()
{
    usize ret = 0;
    for (const auto* pool = get_pool().get(); pool; pool = pool->_next.get())
        ret++;
    return ret;
}

namespace {

constexpr u32 num_threads = 4, num_rounds = 100'000, num_kept_results = 5, num_points = 3;

point written_point(u32 job, u32 round, u32 tag, u32 i)
{
    return { chunk_coords_{(i16)job, (i16)(round & 0x7fff), 0}, local_coords{(u8)(round >> 15 & 15), (u8)tag},
             Vector2b{(i8)i, 0} };
}

void write_points(path_search_result& r, u32 job, u32 round, u32 tag)
{
    for (u32 i = 0; i < num_points; i++)
        arrayAppend(r.raw_path(), written_point(job, round, tag, i));
    arrayAppend(r.smoothings(), path_search_result::waypoint{ written_point(job, round, tag, num_points), round });
}

void check_points(const path_search_result& r, u32 job, u32 round, u32 tag)
{
    const auto path = r.path();
    const auto smoothings = r.smoothings();
    fm_assert(path.size() == num_points);
    for (u32 i = 0; i < num_points; i++)
        fm_assert(path[i] == written_point(job, round, tag, i));
    fm_assert(smoothings.size() == 1);
    fm_assert(smoothings[0].pt == written_point(job, round, tag, num_points));
    fm_assert(smoothings[0].raw_index == round);
}

void run_rounds(u32 job)
{
    Optional<path_search_result> kept[num_kept_results];
    u32 kept_rounds[num_kept_results] = {};
    u32 lcg_state = job;

    for (u32 round = 0; round < num_rounds; round++)
    {
        path_search_result a;
        write_points(a, job, round, 0);
        path_search_result b{move(a)};
        path_search_result c;
        write_points(c, job, round, 1);
        c = move(b);

        fm_assert(!psrpa::get_node(a));
        fm_assert(!psrpa::get_node(b));
        check_points(c, job, round, 0);
        for (u32 i = 0; i < num_kept_results; i++)
            if (kept[i])
                check_points(*kept[i], job, kept_rounds[i], 0);

        lcg_state = lcg_state * 1664525 + 1013904223;
        const u32 i = (lcg_state >> 16) % num_kept_results;
        kept[i] = NullOpt;
        kept[i].emplace(move(c));
        kept_rounds[i] = round;
    }
}

Array<const node*> free_list_nodes(usize max_count)
{
    Array<const node*> ret;
    arrayReserve(ret, max_count);
    for (const auto* n = psrpa::get_pool().get(); n && ret.size() < max_count; n = n->_next.get())
        arrayAppend(ret, n);
    return ret;
}

} // namespace

void Test::test_astar_pool()
{
    const auto& pool = psrpa::get_pool();

    // dijkstra and astar leave nodes in the pool, and so does this test on a second pass
    psrpa::clear_pool();
    fm_assert(psrpa::pool_size() == 0);

    auto a = path_search_result{};
    fm_assert(psrpa::pool_size() == 0);
    fm_assert(!psrpa::get_node(a));

    a = {};
    fm_assert(!psrpa::get_node(a));
    fm_assert(psrpa::pool_size() == 0);

    a = path_search_result{};
    (void)a.raw_path();
    fm_assert(psrpa::get_node(a));
    fm_assert(psrpa::pool_size() == 0);
    const void* const first = psrpa::get_node(a).get();
    fm_assert(first);

    a = {};
    fm_assert(!psrpa::get_node(a));
    fm_assert(psrpa::pool_size() == 1);
    fm_assert(pool.get() == first);

    arrayAppend(a.smoothings(), path_search_result::waypoint{ {}, 0 });
    a = {};

    a = path_search_result{};
    (void)a.raw_path();
    fm_assert(psrpa::get_node(a));
    fm_assert(psrpa::get_node(a).get() == first);
    fm_assert(psrpa::pool_size() == 0);
    fm_assert(a.smoothings().isEmpty());

    auto b = path_search_result{};
    (void)b.raw_path();

    fm_assert(psrpa::get_node(a));
    fm_assert(psrpa::get_node(b));
    fm_assert(psrpa::pool_size() == 0);

    b = {};
    a = {};

    fm_assert(!psrpa::get_node(a));
    fm_assert(!psrpa::get_node(b));
    fm_assert(psrpa::pool_size() == 2);
    fm_assert(pool.get() == first);
}

void Test::test_search_result_threads()
{
    {
        auto results = Array<path_search_result>{ValueInit, 64};
        for (auto& r : results)
            (void)r.raw_path();
    }
    const usize num_free_nodes = psrpa::pool_size();
    fm_assert(num_free_nodes >= 64);

    {
        ThreadPool thread_pool{num_threads};
        thread_pool.parallel_for(num_threads, 1, [](u32 begin, u32 end, u32) {
            fm_assert(end == begin + 1);
            run_rounds(begin);
        });
    }

    auto nodes = free_list_nodes(2*num_free_nodes);
    std::sort(nodes.begin(), nodes.end());
    fm_assert(std::adjacent_find(nodes.begin(), nodes.end()) == nodes.end());
    fm_assert(psrpa::pool_size() == num_free_nodes);
}

} // namespace floormat
