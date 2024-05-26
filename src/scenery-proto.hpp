#pragma once
#include "scenery-type.hpp"
#include "object.hpp"
#include <variant>

namespace floormat {

struct generic_scenery_proto
{
    bool active      : 1 = false;
    bool interactive : 1 = false;

    bool operator==(const generic_scenery_proto& p) const;
    static enum scenery_type scenery_type();
};

struct door_scenery_proto
{
    bool active      : 1 = false;
    bool interactive : 1 = true;
    bool closing     : 1 = false;

    bool operator==(const door_scenery_proto& p) const;
    static enum scenery_type scenery_type();
};

using scenery_proto_variants = std::variant<std::monostate, generic_scenery_proto, door_scenery_proto>;

struct scenery_proto : object_proto
{
    scenery_proto_variants subtype;

    scenery_proto() noexcept;
    ~scenery_proto() noexcept override;
    explicit operator bool() const;
    bool operator==(const object_proto& proto) const override;
    enum scenery_type scenery_type() const;
    scenery_proto(const scenery_proto&) noexcept;
    scenery_proto& operator=(const scenery_proto&) noexcept;
    scenery_proto(scenery_proto&&) noexcept;
    scenery_proto& operator=(scenery_proto&&) noexcept;
};

template<> struct scenery_type_<generic_scenery_proto> : std::integral_constant<scenery_type, scenery_type::generic> {};
template<> struct scenery_type_<door_scenery_proto> : std::integral_constant<scenery_type, scenery_type::door> {};

} // namespace floormat
