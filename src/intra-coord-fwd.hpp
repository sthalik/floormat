#pragma once

namespace floormat {

struct intra_coord_base
{
    enum class Type : uint8_t { Checking, Wrapping };
    static constexpr Type Checking = Type::Checking;
    static constexpr Type Wrapping = Type::Wrapping;

    constexpr bool operator==(const intra_coord_base&) const = default;
};

template<intra_coord_base::Type TYPE = intra_coord_base::Checking> struct basic_intra_coord;
using intra_coord = basic_intra_coord<>;

} // namespace floormat
