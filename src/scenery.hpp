#pragma once
#include "object.hpp"
#include "scenery-type.hpp"

namespace floormat {

class chunk;
class anim_atlas;
class world;
struct scenery_proto;
struct generic_scenery_proto;
struct door_scenery_proto;

struct scenery : object
{
    float depth_offset() const override;
    enum object_type type() const noexcept override;
    virtual enum scenery_type scenery_type() const = 0;
    virtual explicit operator scenery_proto() const; // todo make protected?

protected:
    scenery(object_id id, class chunk& c, const scenery_proto& proto);
};

struct generic_scenery final : scenery
{
    bool active      : 1 = false;
    bool interactive : 1 = false;

    void update(size_t& i, const Ns& dt) override;
    Vector2 ordinal_offset(Vector2b offset) const override;
    bool can_activate(size_t i) const override;
    bool activate(size_t i) override;

    enum scenery_type scenery_type() const override;
    explicit operator scenery_proto() const override;
    explicit operator generic_scenery_proto() const;

private:
    friend class world;
    generic_scenery(object_id id, class chunk& c, const generic_scenery_proto& p, const scenery_proto& p0);
};

struct door_scenery final : scenery
{
    bool closing     : 1 = false;
    bool active      : 1 = false;
    bool interactive : 1 = false;

    void update(size_t& i, const Ns& dt) override;
    Vector2 ordinal_offset(Vector2b offset) const override;
    bool can_activate(size_t i) const override;
    bool activate(size_t i) override;

    enum scenery_type scenery_type() const override;
    explicit operator scenery_proto() const override;
    explicit operator door_scenery_proto() const;

private:
    friend class world;
    door_scenery(object_id id, class chunk& c, const door_scenery_proto& p, const scenery_proto& p0);
};

template<> struct object_type_<scenery> : std::integral_constant<object_type, object_type::scenery> {};
template<> struct object_type_<generic_scenery> : std::integral_constant<object_type, object_type::scenery> {};
template<> struct object_type_<door_scenery> : std::integral_constant<object_type, object_type::scenery> {};
template<> struct object_type_<scenery_proto> : std::integral_constant<object_type, object_type::scenery> {};

template<> struct scenery_type_<generic_scenery> : std::integral_constant<scenery_type, scenery_type::generic> {};
template<> struct scenery_type_<door_scenery> : std::integral_constant<scenery_type, scenery_type::door> {};

} // namespace floormat
