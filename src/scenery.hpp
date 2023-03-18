#pragma once
#include "pass-mode.hpp"
#include "tile-defs.hpp"
#include "rotation.hpp"
#include "entity.hpp"
#include <memory>
#include <type_traits>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Magnum.h>

namespace floormat {

struct chunk;
struct anim_atlas;
struct world;

enum class scenery_type : unsigned char {
    none, generic, door,
};
constexpr inline size_t scenery_type_BITS = 3;

struct scenery_proto : entity_proto
{
    scenery_type sc_type : scenery_type_BITS = scenery_type::none;
    unsigned char active      : 1 = false;
    unsigned char closing     : 1 = false;
    unsigned char interactive : 1 = false;

    scenery_proto();
    scenery_proto(const scenery_proto&);
    ~scenery_proto() noexcept override;
    scenery_proto& operator=(const scenery_proto&);
    bool operator==(const entity_proto& proto) const override;
    operator bool() const;
};

struct scenery final : entity
{
    scenery_type sc_type      : 3 = scenery_type::none;
    unsigned char active      : 1 = false;
    unsigned char closing     : 1 = false;
    unsigned char interactive : 1 = false;

    bool can_activate(size_t i) const override;
    bool activate(size_t i) override;
    bool update(size_t i, float dt) override;
    explicit operator scenery_proto() const;

private:
    friend struct world;
    scenery(object_id id, struct chunk& c, entity_type type, const scenery_proto& proto);
};

template<> struct entity_type_<scenery> : std::integral_constant<entity_type, entity_type::scenery> {};
template<> struct entity_type_<scenery_proto> : std::integral_constant<entity_type, entity_type::scenery> {};

} // namespace floormat
