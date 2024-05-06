#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/vector-wrapper.hpp"
#include "src/search-result.hpp"
#include "src/search-node.hpp"

namespace floormat {

namespace { struct Test_PathPool {}; }

using psrpa = path_search_result_pool_access<Test_PathPool>;
using node = path_search_result::node;

template<>
struct path_search_result_pool_access<Test_PathPool> final
{
    static const Pointer<node>& get_node(const path_search_result& p) { return p._node; }
    static const auto& get_pool() { return path_search_result::_pool; }
    static size_t pool_size();
};

size_t path_search_result_pool_access<Test_PathPool>::pool_size()
{
    size_t ret = 0;
    for (const auto* pool = get_pool().get(); pool; pool = pool->_next.get())
        ret++;
    return ret;
}

void Test::test_astar_pool()
{
    const auto& pool = psrpa::get_pool();
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

    a = path_search_result{};
    (void)a.raw_path();
    fm_assert(psrpa::get_node(a));
    fm_assert(psrpa::get_node(a).get() == first);
    fm_assert(psrpa::pool_size() == 0);

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

} // namespace floormat
