#pragma once
#include "order.hpp"
#include <cr/Array.h>

namespace floormat {

struct order_list_access;
struct grid_order;

// The orders one thread gave, grouped by the object that gave them. Only that thread writes it.
class order_buffer
{
    friend class order_list;
    friend struct order_list_access;

    struct object_orders { u32 update_index, first_order, order_count, first_dependency; };

    Array<order> _orders;
    Array<u32> _dependency_counts;
    Array<u32> _dependencies;
    Array<object_orders> _object_orders;
    bool _is_object_open = false;

public:
    order_buffer();

    // update_index is the object's position among the frame's objects that give orders. Keep the indices
    // dense, since order_list::append_buffers() makes a slot for each index up to the largest.
    void begin_object(u32 update_index);
    // Returns the order's position among its object's orders, from 0. Each dependency is such a position.
    u32 add(const order& o, ArrayView<const u32> dependencies = {});
    void end_object();
    void clear();
    bool empty() const;
};

// What a thread running on_think() writes besides the script's own state.
struct think_context
{
    order_buffer& orders;
    Array<grid_order>& grid_orders;
};

class order_list
{
    friend struct order_list_access;

    struct buffer_object { const order_buffer* buffer; u32 object_index; };

    Array<order> _orders;
    // Slot k holds the buffer object whose update_index is k, or a null buffer.
    Array<buffer_object> _buffer_objects;
    u32 _num_finished_orders = 0;
    u32 _running_position = (u32)-1;

public:
    order_list();

    // Appends the buffers' orders by ascending update_index, each object's in the order it gave them.
    void append_buffers(ArrayView<const order_buffer> buffers);
    // Returns the order's position in the list. An order with giver 0 added by run_all's fn gets the giver of
    // the order running.
    u32 add(const order& o, ArrayView<const u32> dependencies = {});
    // Calls fn(order, position) on each order not yet run, by ascending position. A dependency always names an
    // earlier position, so it has finished. fn may call add().
    template<typename F> void run_all(F&& fn);
    void clear();
    u32 size() const;
    const order& operator[](u32 position) const;
};

} // namespace floormat
