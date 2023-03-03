#pragma once
#include "chunk.hpp"

namespace floormat {

template<typename F>
requires requires(F fun) { fun(); }
void chunk::with_scenery_update(std::size_t idx, F&& fun)
{
    static_assert(std::is_convertible_v<decltype(fun()), bool> || std::is_same_v<void, decltype(fun())>);

    const auto& s = scenery_at(idx), s0 = s;
    bbox bb0; bool b0 = _bbox_for_scenery(idx, bb0);

    bool modified = true;
    if constexpr(!std::is_same_v<void, std::decay_t<decltype(fun())>>)
        modified = fun();
    else
        fun();
    if (!modified)
        return;

    if (bbox bb; !is_passability_modified())
        if (bool b = _bbox_for_scenery(idx, bb);
            b != b0 || scenery::is_collision_modified(s0, s))
            _replace_bbox(bb0, bb, b0, b);
    if (!is_scenery_modified() && scenery::is_mesh_modified(s0, s))
        mark_scenery_modified(false);
}

} // namespace floormat
