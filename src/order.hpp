#pragma once
#include "object-id.hpp"
#include <type_traits>

namespace floormat {

enum class order_kind : u8
{
    none,           // changes nothing
    update_object,  // calls the giver's object::update(); for objects that decide nothing
    script_order,   // the giver's script runs it in on_order()
    move_by_keys,   // moves a playable critter the way its held movement keys point
    COUNT,
};

struct order
{
    // The object whose think() or order gave this one, or 0 for none.
    object_id giver = 0;
    order_kind kind = order_kind::none;
    u8 script_order_kind = 0;   // for order_kind::script_order: the giving script's own number for the order
};

static_assert(std::is_aggregate_v<order>);
static_assert(std::is_trivially_copyable_v<order>);

} // namespace floormat
