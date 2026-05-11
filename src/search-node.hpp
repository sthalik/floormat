#pragma once
#include "compat/defs.hpp"
#include "search-result.hpp"
#include "point.hpp"
#include <cr/Array.h>
#include <cr/Pointer.h>

namespace floormat {

template<typename T> struct path_search_result_pool_access;

struct path_search_result::node
{
    friend struct path_search_result;
    template<typename T> friend struct path_search_result_pool_access;

    node() noexcept;
    fm_DISABLE_COPY(node);
    fm_DEFAULT_MOVE_(node);

    Array<point> vec;
    Pointer<node> _next;
};

} // namespace floormat
