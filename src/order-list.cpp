#include "order-list.hpp"
#include "compat/assert.hpp"
#include <cr/GrowableArray.h>

namespace floormat {

namespace {

template<typename T> void reserve_with_headroom(Array<T>& A, usize count)
{
    if (arrayCapacity(A) < count)
        arrayReserve(A, count + count/2);
}

} // namespace

order_buffer::order_buffer()
{
    arrayReserve(_orders, 16);
    arrayReserve(_object_orders, 16);
}

void order_buffer::clear()
{
    fm_assert(!_is_object_open);
    arrayClear(_orders);
    arrayClear(_object_orders);
}

bool order_buffer::empty() const
{
    return _orders.isEmpty();
}

order_list::order_list()
{
    arrayReserve(_orders, 16);
    arrayReserve(_buffer_objects, 16);
}

void order_list::append_buffers(ArrayView<const order_buffer> buffers)
{
    u32 num_slots = 0;
    usize num_orders = 0;
    for (const auto& b : buffers)
    {
        fm_assert(!b._is_object_open);
        for (const auto& object : b._object_orders)
            if (object.update_index >= num_slots)
                num_slots = object.update_index + 1;
        num_orders += b._orders.size();
    }
    arrayClear(_buffer_objects);
    reserve_with_headroom(_buffer_objects, num_slots);
    arrayResize(_buffer_objects, ValueInit, num_slots);
    reserve_with_headroom(_orders, _orders.size() + num_orders);

    for (const auto& b : buffers)
        for (auto i = 0u; i < (u32)b._object_orders.size(); i++)
        {
            auto& slot = _buffer_objects[b._object_orders[i].update_index];
            fm_assert(!slot.buffer);
            slot = buffer_object{&b, i};
        }

    for (const auto& [b, object_index] : _buffer_objects)
    {
        if (!b)
            continue;
        const auto& object = b->_object_orders[object_index];
        arrayAppend(_orders, b->_orders.sliceSize(object.first_order, object.order_count));
    }
}

u32 order_list::add(const order& o)
{
    const auto position = (u32)_orders.size();
    auto stored = o;
    if (!stored.giver && _running_position != (u32)-1)
        stored.giver = _orders[_running_position].giver;
    arrayAppend(_orders, stored);
    return position;
}

void order_list::clear()
{
    fm_assert(_running_position == (u32)-1);
    arrayClear(_orders);
    arrayClear(_buffer_objects);
    _num_finished_orders = 0;
}

u32 order_list::size() const
{
    return (u32)_orders.size();
}

const order& order_list::operator[](u32 position) const
{
    return _orders[position];
}

} // namespace floormat
