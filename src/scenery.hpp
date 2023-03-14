#pragma once
#include "pass-mode.hpp"
#include "tile-defs.hpp"
#include "rotation.hpp"
#include "entity.hpp"
#include <cstdint>
#include <memory>
#include <type_traits>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Magnum.h>

namespace floormat {

struct chunk;
struct anim_atlas;
struct world;

enum class scenery_type : std::uint8_t {
    none, generic, door,
};

struct scenery_proto : entity_proto
{
    scenery_type sc_type     : 3 = scenery_type::none;
    std::uint8_t active      : 1 = false;
    std::uint8_t closing     : 1 = false;
    std::uint8_t interactive : 1 = false;

    scenery_proto();
    scenery_proto(const scenery_proto&);
    ~scenery_proto() noexcept override;
    scenery_proto& operator=(const scenery_proto&);
    operator bool() const;
};

struct scenery final : entity
{
    scenery_type sc_type     : 3 = scenery_type::none;
    std::uint8_t active      : 1 = false;
    std::uint8_t closing     : 1 = false;
    std::uint8_t interactive : 1 = false;

    bool can_activate(It it, struct chunk& c) const override;
    bool activate(It it, struct chunk& c) override;
    bool update(It it, struct chunk& c, float dt) override;
    bool operator==(const entity_proto& p) const override;

private:
    friend struct world;
    scenery(std::uint64_t id, struct world& w, entity_type type, const scenery_proto& proto);
};

template<> struct entity_type_<scenery> : std::integral_constant<entity_type, entity_type::scenery> {};

} // namespace floormat
