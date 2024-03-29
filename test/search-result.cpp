#include "app.hpp"
#include "compat/assert.hpp"
#include "src/search-result.hpp"
#include "src/search-node.hpp"

namespace floormat {

void test_app::test_astar_pool()
{
    auto& pool = path_search_result::_pool;
    fm_assert(!pool);
    {
        auto a = path_search_result{};
        fm_assert(!pool);
    }
    fm_assert(pool);
    auto* pool2 = pool.get();
    {
        auto b = path_search_result{};
        fm_assert(b._node.get() == pool2);
        auto c = path_search_result{};
        fm_assert(!pool);
        fm_assert(c._node.get() != pool2);
        fm_assert(b._node.get() == pool2);
        fm_assert(!b._node->_next);
        fm_assert(!c._node->_next);
    }
    {
        auto count = 0uz;
        for (const auto* ptr = pool.get(); ptr; ptr = ptr->_next.get())
            count++;
        fm_assert(count == 2);
    }
}

} // namespace floormat
