#pragma once
#include "src/light-falloff.hpp"
#include "src/object.hpp"
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Color.h>

namespace floormat {

struct light_proto : object_proto
{
    light_proto();
    light_proto(const light_proto&);
    light_proto& operator=(const light_proto&);
    ~light_proto() noexcept override;
    bool operator==(const light_proto&) const;

    float max_distance = 0;
    Color4ub color{255, 255, 255, 255};
    light_falloff falloff : 3 = light_falloff::linear;
    uint8_t enabled : 1 = true;
};

struct light final : object
{
    float max_distance;
    Color4ub color;
    light_falloff falloff : 2;
    uint8_t enabled : 1;

    light(object_id id, struct chunk& c, const light_proto& proto);

    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;
    object_type type() const noexcept override;
    void update(size_t i, float dt) override;
    bool is_dynamic() const override;
    bool is_virtual() const override;

    explicit operator light_proto() const;

    friend struct world;
};

template<> struct object_type_<struct light> : std::integral_constant<object_type, object_type::light> {};
template<> struct object_type_<struct light_proto> : std::integral_constant<object_type, object_type::light> {};

} // namespace floormat
