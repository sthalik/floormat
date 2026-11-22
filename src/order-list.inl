#pragma once
#include "order-list.hpp"
#include "compat/assert.hpp"

namespace floormat {

template<typename F>
void order_list::run_all(F&& fn)
{
    fm_assert(_running_position == (u32)-1);
    while (!_ready_heap.isEmpty())
    {
        const auto position = start_next_order();
        fn(order{_orders[position]}, position);
        finish_running_order();
    }
    for (const auto& s : _order_states)
        fm_assert(s.is_finished);
}

} // namespace floormat
