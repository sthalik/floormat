#pragma once
#include "pass-mode.hpp"
#include "tile-defs.hpp"
#include "rotation.hpp"
#include "object.hpp"
#include <type_traits>
#include <variant>
#include <Corrade/Containers/Optional.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Magnum.h>

namespace floormat {

template<typename... Ts> struct [[maybe_unused]] overloaded : Ts... { using Ts::operator()...; };
template<typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct chunk;
class anim_atlas;
class world;

enum class scenery_type : unsigned char {
    none, generic, door,
};

struct generic_scenery_proto
{
    unsigned char active      : 1 = false;
    unsigned char interactive : 1 = false;

    bool operator==(const generic_scenery_proto& p) const;
    enum scenery_type scenery_type() const;
};

struct door_scenery_proto
{
    unsigned char active      : 1 = false;
    unsigned char interactive : 1 = false;
    unsigned char closing     : 1 = false;

    bool operator==(const door_scenery_proto& p) const;
    enum scenery_type scenery_type() const;
};

using scenery_proto_variants = std::variant<generic_scenery_proto, door_scenery_proto>;

struct scenery_proto : object_proto
{
    scenery_proto_variants subtype;

    scenery_proto();
    scenery_proto(const scenery_proto&);
    ~scenery_proto() noexcept override;
    scenery_proto& operator=(const scenery_proto&);
    bool operator==(const object_proto& proto) const override;
    explicit operator bool() const;
    enum scenery_type scenery_type() const;
};

struct scenery;

struct generic_scenery
{
    unsigned char active      : 1 = false;
    unsigned char interactive : 1 = false;

    void update(scenery& sc, size_t i, float dt);
    Vector2 ordinal_offset(const scenery& sc, Vector2b offset) const;
    bool can_activate(const scenery& sc, size_t i) const;
    bool activate(scenery& sc, size_t i);

    object_type type() const noexcept;
    enum scenery_type scenery_type() const;
    explicit operator generic_scenery_proto() const;

    generic_scenery(object_id id, struct chunk& c, const generic_scenery_proto& p);
};

struct door_scenery
{
    unsigned char closing     : 1 = false;
    unsigned char active      : 1 = false;
    unsigned char interactive : 1 = false;

    void update(scenery& sc, size_t i, float dt);
    Vector2 ordinal_offset(const scenery& sc, Vector2b offset) const;
    bool can_activate(const scenery& sc, size_t i) const;
    bool activate(scenery& sc, size_t i);

    object_type type() const noexcept;
    enum scenery_type scenery_type() const;
    explicit operator door_scenery_proto() const;

    door_scenery(object_id id, struct chunk& c, const door_scenery_proto& p);
};

using scenery_variants = std::variant<generic_scenery, door_scenery>;

struct scenery final : object
{
    scenery_variants subtype;

    void update(size_t i, float dt) override;
    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;
    bool can_activate(size_t i) const override;
    bool activate(size_t i) override;

    object_type type() const noexcept override;
    explicit operator scenery_proto() const;
    enum scenery_type scenery_type() const;

    static scenery_variants subtype_from_proto(object_id id, struct chunk& c,
                                               const scenery_proto_variants& variants);
    static scenery_variants subtype_from_scenery_type(object_id id, struct chunk& c, enum scenery_type type);

private:
    friend class world;
    scenery(object_id id, struct chunk& c, const scenery_proto& proto);
};

template<> struct object_type_<scenery> : std::integral_constant<object_type, object_type::scenery> {};
template<> struct object_type_<scenery_proto> : std::integral_constant<object_type, object_type::scenery> {};

} // namespace floormat
