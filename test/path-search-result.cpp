#include "app.hpp"
#include "compat/assert.hpp"
#include "path-search-result.hpp"

namespace floormat {

void test_app::test_path_search_node_pool()
{
    auto& pool = path_search_result::_pool;
    fm_debug_assert(!pool);
    {
        auto a = path_search_result{};
        fm_debug_assert(!pool);
    }
    fm_debug_assert(pool);
    {
        auto* pool2 = pool.get();
        auto b = path_search_result{};
        fm_debug_assert(b._node.get() == pool2);
        auto c = path_search_result{};
        fm_debug_assert(c._node.get() != pool2);
        fm_debug_assert(b._node.get() == pool2);
    }
}

} // namespace floormat
