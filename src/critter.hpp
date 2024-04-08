#pragma once
#include "global-coords.hpp"
#include "object.hpp"
#include "script.hpp"
#include <Corrade/Containers/String.h>

namespace floormat {

class anim_atlas;
class world;
struct critter_script;

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

    ~critter() noexcept override;
    object_type type() const noexcept override;
    explicit operator critter_proto() const;

    void update(size_t& i, const Ns& dt) override;
    void update_movement(size_t& i, const Ns& dt, rotation r);
    void update_nonplayable(size_t& i, const Ns& dt);

    struct move_result { bool blocked, moved; };
    [[nodiscard]] move_result move_toward(size_t& i, const Ns& dt, const point& dest);

    void set_keys(bool L, bool R, bool U, bool D);
    void set_keys_auto();
    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;

    Script<critter_script, critter> script;
    String name;
    float speed = 1;
    uint16_t offset_frac_ = 0; // todo! remove underscore

    struct movement_s
    {
        bool L : 1 = false;
        bool R : 1 = false;
        bool U : 1 = false;
        bool D : 1 = false;
        bool AUTO : 1 = false;

        bool _pad1 : 1;
        bool _pad2 : 1;
        bool _pad3 : 1;
        //bool _pad4 : 1;
    };

    union
    {
        uint8_t movement_value = 0;
        movement_s movement;
    };

    bool playable : 1 = false;

private:
    friend class world;
    critter(object_id id, class chunk& c, critter_proto proto);
};

template<> struct object_type_<struct critter> : std::integral_constant<object_type, object_type::critter> {};
template<> struct object_type_<struct critter_proto> : std::integral_constant<object_type, object_type::critter> {};

} // namespace floormat
