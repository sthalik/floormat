#pragma once
#include "object.hpp"
#include <variant>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Magnum.h>

namespace floormat {

class chunk;
class anim_atlas;
class world;

enum class scenery_type : unsigned char {
    none, generic, door, COUNT,
};

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

using scenery_proto_variants = std::variant<generic_scenery_proto, door_scenery_proto>;

struct scenery_proto : object_proto
{
    scenery_proto_variants subtype;

    scenery_proto() noexcept;
    scenery_proto(const scenery_proto&) noexcept;
    ~scenery_proto() noexcept override;
    scenery_proto& operator=(const scenery_proto&) noexcept;
    bool operator==(const object_proto& proto) const override;
    explicit operator bool() const;
    enum scenery_type scenery_type() const;
};

struct scenery;

struct scenery : object
{
    float depth_offset() const override;
    enum object_type type() const noexcept override;
    virtual enum scenery_type scenery_type() const = 0;

protected:
    virtual explicit operator scenery_proto() const;
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

} // namespace floormat
