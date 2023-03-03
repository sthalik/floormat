#pragma once
#include "chunk.hpp"

namespace floormat {

template<typename F> void chunk::with_scenery_bbox_update(std::size_t i, F&& fun)
{
    static_assert(std::is_invocable_v<F>);
    static_assert(std::is_convertible_v<decltype(fun()), bool> || std::is_same_v<void, decltype(fun())>);
    if (is_passability_modified())
    {
        auto& s = scenery_at(i);
        auto r0 = s.r;
        bool modified = true;
        if constexpr(!std::is_same_v<void, std::decay_t<decltype(fun())>>)
            modified = fun();
        else
            fun();
        if (r0 != s.r)
        {
            fm_debug_assert(modified);
            mark_scenery_modified();
        }
    }
    else
    {
        bbox x0, x;
        bool b0 = _bbox_for_scenery(i, x0);
        if constexpr(std::is_same_v<void, std::decay_t<decltype(fun())>>)
        {
            fun();
            _replace_bbox(x0, x, b0, _bbox_for_scenery(i, x));
        }
        else
        {
            if (fun())
                _replace_bbox(x0, x, b0, _bbox_for_scenery(i, x));
        }
    }
}

} // namespace floormat
