#pragma once
#include "object-id.hpp"
#include <type_traits>

namespace floormat {

// An order of kind none changes nothing.
enum class order_kind : u8 { none, COUNT, };

struct order
{
    // The object whose think() or order gave this one, or 0 for none.
    object_id giver = 0;
    order_kind kind = order_kind::none;
};

static_assert(std::is_aggregate_v<order>);
static_assert(std::is_trivially_copyable_v<order>);

} // namespace floormat
