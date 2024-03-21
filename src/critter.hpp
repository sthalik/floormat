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

    void update(size_t i, Ns dt) override;
    void update_movement(size_t i, Ns dt, rotation r);
    void update_nonplayable(size_t i, Ns dt);
    void set_keys(bool L, bool R, bool U, bool D);
    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;

    String name;
    float speed = 1;
    Vector2us offset_frac; // todo! switch to Vector2ui due to `allocate_frame_time'
    bool b_L : 1 = false, b_R : 1 = false, b_U : 1 = false, b_D : 1 = false;
    bool playable : 1 = false;

private:
    friend class world;
    critter(object_id id, class chunk& c, critter_proto proto);
};

template<> struct object_type_<struct critter> : std::integral_constant<object_type, object_type::critter> {};
template<> struct object_type_<struct critter_proto> : std::integral_constant<object_type, object_type::critter> {};

} // namespace floormat
