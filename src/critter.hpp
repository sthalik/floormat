#pragma once
#include "src/global-coords.hpp"
#include "src/object.hpp"
#include <Corrade/Containers/String.h>

namespace floormat {

class anim_atlas;
class world;

struct critter_proto : object_proto
{
    String name;
    float speed = 1;
    bool playable : 1 = false;

    critter_proto();
    critter_proto(const critter_proto&);
    ~critter_proto() noexcept override;
    critter_proto& operator=(const critter_proto&);
    bool operator==(const object_proto& proto) const override;
};

struct critter final : object
{
    static constexpr double framerate = 60, move_speed = 60;
    static constexpr double frame_time = 1/framerate;

    object_type type() const noexcept override;
    explicit operator critter_proto() const;

    void update(size_t i, const Ns& dt) override;
    void update_movement(size_t i, const Ns& dt, rotation r);
    void update_nonplayable(size_t i, const Ns& dt);
    void set_keys(bool L, bool R, bool U, bool D);
    void set_keys_auto();
    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;

    String name;
    float speed = 1;
    uint16_t offset_frac_ = 0;

    struct movement_s {
        bool L : 1 = false,
             R : 1 = false,
             U : 1 = false,
             D : 1 = false,
             AUTO : 1 = false;
        bool _pad1 : 1 = false,
             _pad2 : 1 = false,
             _pad3 : 1 = false;
    } movement;

    bool playable : 1 = false;

private:
    friend class world;
    critter(object_id id, class chunk& c, critter_proto proto);
};

template<> struct object_type_<struct critter> : std::integral_constant<object_type, object_type::critter> {};
template<> struct object_type_<struct critter_proto> : std::integral_constant<object_type, object_type::critter> {};

} // namespace floormat
