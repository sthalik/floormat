#pragma once
#include "order-list.hpp"
#include "compat/assert.hpp"

namespace floormat {

template<typename F>
void order_list::run_all(F&& fn)
{
    fm_assert(_running_position == (u32)-1);
    for (; _num_finished_orders < (u32)_orders.size(); _num_finished_orders++)
    {
        const auto position = _num_finished_orders;
        _running_position = position;
        fn(order{_orders[position]}, position);
        _running_position = (u32)-1;
    }
}

} // namespace floormat
