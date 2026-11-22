#pragma once
#include "order.hpp"
#include <cr/Array.h>

namespace floormat {

struct order_list_access;

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

    // update_index is the object's position in the frame's update list.
    void begin_object(u32 update_index);
    // Returns the order's position among its object's orders, from 0. Each dependency is such a position.
    u32 add(const order& o, ArrayView<const u32> dependencies = {});
    void end_object();
    void clear();
    bool empty() const;
};

class order_list
{
    friend struct order_list_access;

    struct order_state
    {
        u32 num_unfinished_dependencies, first_dependent_node;
        bool is_finished;
    };
    struct dependent_node { u32 dependent, next; };
    struct buffer_object { const order_buffer* buffer; u32 object_index; };

    Array<order> _orders;
    Array<order_state> _order_states;
    Array<dependent_node> _dependent_nodes;
    Array<u32> _ready_heap;
    Array<buffer_object> _buffer_objects;
    u32 _running_position = (u32)-1;

    u32 add_impl(const order& o, ArrayView<const u32> dependencies, u32 dependency_offset);
    void push_ready_order(u32 position);
    u32 start_next_order();
    void finish_running_order();

public:
    order_list();

    // Appends the buffers' orders by ascending update_index, each object's in the order it gave them.
    void append_buffers(ArrayView<const order_buffer> buffers);
    // Returns the order's position in the list. An order with giver 0 added by run_all's fn gets the giver of
    // the order running.
    u32 add(const order& o, ArrayView<const u32> dependencies = {});
    // Calls fn(order, position) on the ready order with the lowest position until none is left. fn may call add().
    template<typename F> void run_all(F&& fn);
    void clear();
    u32 size() const;
    const order& operator[](u32 position) const;
};

} // namespace floormat
