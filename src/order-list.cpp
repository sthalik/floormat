#include "order-list.hpp"
#include "compat/assert.hpp"
#include <algorithm>
#include <functional>
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
    arrayReserve(_dependency_counts, 16);
    arrayReserve(_dependencies, 16);
    arrayReserve(_object_orders, 16);
}

void order_buffer::begin_object(u32 update_index)
{
    fm_assert(!_is_object_open);
    _is_object_open = true;
    arrayAppend(_object_orders, object_orders{update_index, (u32)_orders.size(), 0, (u32)_dependencies.size()});
}

u32 order_buffer::add(const order& o, ArrayView<const u32> dependencies)
{
    fm_assert(_is_object_open);
    auto& object = _object_orders.back();
    const auto position = object.order_count;
    for (auto d : dependencies)
        fm_assert(d < position);
    arrayAppend(_orders, o);
    arrayAppend(_dependency_counts, (u32)dependencies.size());
    arrayAppend(_dependencies, dependencies);
    object.order_count++;
    return position;
}

void order_buffer::end_object()
{
    fm_assert(_is_object_open);
    _is_object_open = false;
}

void order_buffer::clear()
{
    fm_assert(!_is_object_open);
    arrayClear(_orders);
    arrayClear(_dependency_counts);
    arrayClear(_dependencies);
    arrayClear(_object_orders);
}

bool order_buffer::empty() const
{
    return _orders.isEmpty();
}

order_list::order_list()
{
    arrayReserve(_orders, 16);
    arrayReserve(_order_states, 16);
    arrayReserve(_dependent_nodes, 16);
    arrayReserve(_ready_heap, 16);
    arrayReserve(_buffer_objects, 16);
}

void order_list::append_buffers(ArrayView<const order_buffer> buffers)
{
    usize num_objects = 0, num_orders = 0, num_dependencies = 0;
    for (const auto& b : buffers)
    {
        fm_assert(!b._is_object_open);
        num_objects += b._object_orders.size();
        num_orders += b._orders.size();
        num_dependencies += b._dependencies.size();
    }
    arrayClear(_buffer_objects);
    reserve_with_headroom(_buffer_objects, num_objects);
    reserve_with_headroom(_orders, _orders.size() + num_orders);
    reserve_with_headroom(_order_states, _order_states.size() + num_orders);
    reserve_with_headroom(_ready_heap, _ready_heap.size() + num_orders);
    reserve_with_headroom(_dependent_nodes, _dependent_nodes.size() + num_dependencies);

    for (const auto& b : buffers)
        for (auto i = 0u; i < (u32)b._object_orders.size(); i++)
            arrayAppend(_buffer_objects, buffer_object{&b, i});

    const auto update_index_of = [](const buffer_object& x) {
        return x.buffer->_object_orders[x.object_index].update_index;
    };
    std::sort(_buffer_objects.begin(), _buffer_objects.end(), [&](const buffer_object& a, const buffer_object& b) {
        return update_index_of(a) < update_index_of(b);
    });
    for (auto i = 1u; i < (u32)_buffer_objects.size(); i++)
        fm_assert(update_index_of(_buffer_objects[i-1]) != update_index_of(_buffer_objects[i]));

    for (const auto& [b, object_index] : _buffer_objects)
    {
        const auto& object = b->_object_orders[object_index];
        const auto first_position = (u32)_orders.size();
        auto dependency_index = object.first_dependency;
        for (auto i = object.first_order; i < object.first_order + object.order_count; i++)
        {
            const auto count = b->_dependency_counts[i];
            add_impl(b->_orders[i], b->_dependencies.sliceSize(dependency_index, count), first_position);
            dependency_index += count;
        }
    }
}

u32 order_list::add(const order& o, ArrayView<const u32> dependencies)
{
    auto stored = o;
    if (!stored.giver && _running_position != (u32)-1)
        stored.giver = _orders[_running_position].giver;
    return add_impl(stored, dependencies, 0);
}

u32 order_list::add_impl(const order& o, ArrayView<const u32> dependencies, u32 dependency_offset)
{
    const auto position = (u32)_orders.size();
    u32 num_unfinished = 0;
    for (auto d : dependencies)
    {
        const auto dependency = d + dependency_offset;
        fm_assert(dependency < position);
        auto& state = _order_states[dependency];
        if (!state.is_finished)
        {
            arrayAppend(_dependent_nodes, dependent_node{position, state.first_dependent_node});
            state.first_dependent_node = (u32)_dependent_nodes.size() - 1;
            num_unfinished++;
        }
    }
    arrayAppend(_orders, o);
    arrayAppend(_order_states, order_state{num_unfinished, (u32)-1, false});
    if (!num_unfinished)
        push_ready_order(position);
    return position;
}

void order_list::push_ready_order(u32 position)
{
    arrayAppend(_ready_heap, position);
    std::push_heap(_ready_heap.begin(), _ready_heap.end(), std::greater<>{});
}

u32 order_list::start_next_order()
{
    std::pop_heap(_ready_heap.begin(), _ready_heap.end(), std::greater<>{});
    const auto position = _ready_heap.back();
    arrayRemoveSuffix(_ready_heap, 1);
    fm_assert(!_order_states[position].is_finished);
    _running_position = position;
    return position;
}

void order_list::finish_running_order()
{
    const auto position = _running_position;
    _running_position = (u32)-1;
    auto& state = _order_states[position];
    state.is_finished = true;
    for (auto i = state.first_dependent_node; i != (u32)-1; i = _dependent_nodes[i].next)
    {
        const auto dependent = _dependent_nodes[i].dependent;
        auto& dependent_state = _order_states[dependent];
        fm_assert(dependent_state.num_unfinished_dependencies > 0);
        if (!--dependent_state.num_unfinished_dependencies)
            push_ready_order(dependent);
    }
}

void order_list::clear()
{
    fm_assert(_running_position == (u32)-1);
    arrayClear(_orders);
    arrayClear(_order_states);
    arrayClear(_dependent_nodes);
    arrayClear(_ready_heap);
    arrayClear(_buffer_objects);
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
