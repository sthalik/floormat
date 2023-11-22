#pragma once
#include "pass-mode.hpp"
#include "tile-defs.hpp"
#include "rotation.hpp"
#include "object.hpp"
#include <type_traits>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Magnum.h>

namespace floormat {

struct chunk;
class anim_atlas;
struct world;

enum class scenery_type : unsigned char {
    none, generic,
    door, // todo remove it
};
constexpr inline size_t scenery_type_BITS = 3;

struct scenery_proto : object_proto
{
    scenery_type sc_type : scenery_type_BITS = scenery_type::none;
    unsigned char active      : 1 = false;
    unsigned char closing     : 1 = false;
    unsigned char interactive : 1 = false;

    scenery_proto();
    scenery_proto(const scenery_proto&);
    ~scenery_proto() noexcept override;
    scenery_proto& operator=(const scenery_proto&);
    bool operator==(const object_proto& proto) const override;
    operator bool() const;
};

struct scenery final : object
{
    scenery_type sc_type      : 3 = scenery_type::none;
    unsigned char active      : 1 = false;
    unsigned char closing     : 1 = false;
    unsigned char interactive : 1 = false;

    void update(size_t i, float dt) override;
    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;
    bool can_activate(size_t i) const override;
    bool activate(size_t i) override;

    object_type type() const noexcept override;
    explicit operator scenery_proto() const;

private:
    friend struct world;
    scenery(object_id id, struct chunk& c, const scenery_proto& proto);
};

template<> struct object_type_<scenery> : std::integral_constant<object_type, object_type::scenery> {};
template<> struct object_type_<scenery_proto> : std::integral_constant<object_type, object_type::scenery> {};

} // namespace floormat
