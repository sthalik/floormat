#include "app.hpp"
#include "compat/assert.hpp"
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
};

void test_app::test_astar_pool()
{
    auto& pool = psrpa::get_pool();
    fm_assert(!pool);
    {
        auto a = path_search_result{};
        fm_assert(!pool);
    }
    fm_assert(pool);
    auto* pool2 = pool.get();
    {
        auto b = path_search_result{};
        fm_assert(psrpa::get_node(b).get() == pool2);
        auto c = path_search_result{};
        fm_assert(!pool);
        fm_assert(psrpa::get_node(c).get() != pool2);
        fm_assert(psrpa::get_node(b).get() == pool2);
        fm_assert(!psrpa::get_node(b)->_next);
        fm_assert(!psrpa::get_node(c)->_next);
    }
    {
        auto count = 0uz;
        for (const auto* ptr = pool.get(); ptr; ptr = ptr->_next.get())
            count++;
        fm_assert(count == 2);
    }
}

} // namespace floormat
