#pragma once
#include "compat/defs.hpp"
#include "search-result.hpp"
#include "point.hpp"
#include <vector>
#include <Corrade/Containers/Pointer.h>

namespace floormat {

struct path_search_result::pair
{
    point pt;
    uint32_t length = 0;
};

struct path_search_result::node
{
    friend struct path_search_result;
    friend struct test_app;

    node() noexcept;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(node);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(node);

    std::vector<point> vec;
    std::vector<pair> vec_;

private:
    Pointer<node> _next;
};

} // namespace floormat
