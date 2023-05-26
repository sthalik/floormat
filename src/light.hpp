#pragma once
#include "src/light-falloff.hpp"
#include "src/entity.hpp"
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Color.h>

namespace floormat {

struct light_proto : entity_proto
{
    light_proto();
    light_proto(const light_proto&);
    light_proto& operator=(const light_proto&);
    ~light_proto() noexcept override;
    bool operator==(const light_proto&) const;

    Vector2 half_dist{3};
    Color3ub color{255, 255, 255};
    light_falloff falloff : 3 = light_falloff::linear;
    uint8_t symmetric : 1 = true;
};

struct light final : entity
{
    Vector2 half_dist;
    Color3ub color;
    light_falloff falloff : 3;
    uint8_t symmetric : 1 = true;

    light(object_id id, struct chunk& c, const light_proto& proto);

    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;
    entity_type type() const noexcept override;
    bool update(size_t i, float dt) override;
    bool is_dynamic() const override;
    bool is_virtual() const override;

    static float calc_intensity(float half_dist, light_falloff falloff);

    friend struct world;
};

} // namespace floormat
