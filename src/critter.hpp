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
    bool playable = false;

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

    void update(const bptr<object>& ptr, size_t& i, const Ns& dt) override;
    void update_movement(size_t& i, const Ns& dt, rotation r);

    struct move_result { bool blocked, moved; };
    [[nodiscard]] move_result move_toward(size_t& i, Ns& dt, const point& dest);

    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;

    void init_script(const bptr<object>& ptr) override;
    void destroy_script_pre(const bptr<object>& ptr, script_destroy_reason r) override;
    void destroy_script_post() override;

    void clear_auto_movement();
    bool maybe_stop_auto_movement();
    void set_keys(bool L, bool R, bool U, bool D);
    void set_keys_auto();

    Script<critter_script, critter> script;
    String name;
    float speed = 1;
    uint16_t offset_frac = 0;

    bool playable : 1 = false;

    struct move_s
    {
        bool L    : 1 = false;
        bool R    : 1 = false;
        bool U    : 1 = false;
        bool D    : 1 = false;
        bool AUTO : 1 = false;
        bool pad1 : 1 = false;
        bool pad2 : 1 = false;
        bool pad3 : 1 = false;
        //bool _pad4 : 1;
    };

    union move_u
    {
        move_s bits;
        uint8_t val = 0;
    };

    union
    {
        move_s moves;
        uint8_t moves_ = 0;
    };

private:
    friend class world;
    critter(object_id id, class chunk& c, critter_proto proto);
};

template<> struct object_type_<struct critter> : std::integral_constant<object_type, object_type::critter> {};
template<> struct object_type_<struct critter_proto> : std::integral_constant<object_type, object_type::critter> {};

} // namespace floormat
