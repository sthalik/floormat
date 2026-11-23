#pragma once
#include "order-list.hpp"
#include "compat/assert.hpp"
#include <cr/GrowableArray.h>

namespace floormat {

inline void order_buffer::begin_object(u32 update_index)
{
    fm_assert(!_is_object_open);
    _is_object_open = true;
    arrayAppend(_object_orders, object_orders{update_index, (u32)_orders.size(), 0});
}

inline u32 order_buffer::add(const order& o, ArrayView<const u32> dependencies)
{
    fm_assert(_is_object_open);
    auto& object = _object_orders.back();
    const auto position = object.order_count;
    for (auto d : dependencies)
        fm_assert(d < position);
    arrayAppend(_orders, o);
    object.order_count++;
    return position;
}

inline void order_buffer::end_object()
{
    fm_assert(_is_object_open);
    _is_object_open = false;
}

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
