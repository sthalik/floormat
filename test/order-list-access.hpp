#pragma once
#include "src/order-list.hpp"
#include <array>
#include <cr/GrowableArray.h>

namespace floormat {

struct order_list_access
{
    static std::array<usize, 2> capacities(order_list& list)
    {
        return { arrayCapacity(list._orders), arrayCapacity(list._buffer_objects) };
    }

    static std::array<usize, 2> capacities(order_buffer& buffer)
    {
        return { arrayCapacity(buffer._orders), arrayCapacity(buffer._object_orders) };
    }
};

} // namespace floormat
