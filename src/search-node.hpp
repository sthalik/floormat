#pragma once
#include "compat/defs.hpp"
#include "search-result.hpp"
#include "point.hpp"
#include <vector>
#include <Corrade/Containers/Pointer.h>

namespace floormat {

template<typename T> struct path_search_result_pool_access;

struct path_search_result::node
{
    friend struct path_search_result;
    template<typename T> friend struct path_search_result_pool_access;

    node() noexcept;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(node);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(node);

    std::vector<point> vec;
    Pointer<node> _next;
};

} // namespace floormat
