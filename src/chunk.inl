#pragma once
#include "chunk.hpp"
#include "scenery.hpp"

namespace floormat {

template<typename F>
requires requires(F fun) { fun(); }
void chunk::with_scenery_update(entity& s, F&& fun)
{
    static_assert(std::is_convertible_v<decltype(fun()), bool> || std::is_same_v<void, decltype(fun())>);

    // todo handle coord & offset fields

    auto ch = s.coord.chunk();
    entity_proto s0 = s;
    bbox bb0; bool b0 = _bbox_for_scenery(s, bb0);

    bool modified = true;
    if constexpr(!std::is_same_v<void, std::decay_t<decltype(fun())>>)
        modified = fun();
    else
        fun();
    if (!modified)
        return;

    if (s.coord.chunk() != ch) // todo
        return;

    if (bbox bb; !is_passability_modified())
        if (bool b = _bbox_for_scenery(s, bb); b != b0 || bb != bb0)
            _replace_bbox(bb0, bb, b0, b);
    if (!is_scenery_modified() && !s.is_dynamic() && s != s0)
        mark_scenery_modified(false);
}

} // namespace floormat
