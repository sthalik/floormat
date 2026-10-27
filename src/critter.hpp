#pragma once
#include "object.hpp"
#include "script.hpp"
#include <cr/String.h>

namespace floormat {

class anim_atlas;
class world;
struct critter_script;

constexpr uint32_t anim_speed_unit = 1u << 16;

struct critter_proto : object_proto
{
    String name;
    float speed = 1;
    float anim_speed = 1;
    bool playable = false;

    critter_proto();
    explicit critter_proto(object_proto base);
    critter_proto(const critter_proto&);
    ~critter_proto() noexcept override;
    critter_proto& operator=(const critter_proto&);
    bool operator==(const object_proto& proto) const override;
};

struct critter final : object
{
    ~critter() noexcept override;
    object_type type() const noexcept override;
    explicit operator critter_proto() const;

    void update(const bptr<object>& ptr, size_t& i, const Ns& dt) override;
    void update_movement(size_t& i, const Ns& dt, rotation r);

    struct move_result
    {
        bool blocked, moved;
        /// Only from the overload taking `from`, with blocked: the critter wasn't on the line.
        bool displaced = false;
    };

    enum class walk_result : uint8_t { none, arrived, blocked, displaced, };

    /// Walks to dest in up to two legs, along an axis until the rest is diagonal, then
    /// diagonally (next_step()). For direct walks without A*.
    /// - Facing follows each leg.
    /// - One collider query covers both legs and their turns, up to half a chunk of steps. The
    ///   walk stops before the first blocked step.
    /// - A diagonal step costs √2 frames. Time below the next step's cost stays in delta.
    /// - On arrival all unspent time goes back in dt.
    /// - Starts from any position.
    [[nodiscard]] move_result move_toward(size_t& i, Ns& dt, const point& dest);

    /// Walks the Bresenham pixels of the segment from `from` to dest, which stay within half a
    /// pixel of it at any angle. For segments of a smoothed path.
    /// - Facing is the nearest of the 8 directions, fixed for the segment.
    /// - A frame's steps are one find_swept_collider() over the line, off-axis pixels of
    ///   diagonal steps included. The critter stops at the last step its box reaches.
    /// - A step costs its length in frames. Time below one step stays in delta.
    /// - The critter must stand on one of the line's pixels. Otherwise it neither moves nor
    ///   turns, and the result has displaced and blocked set.
    [[nodiscard]] move_result move_toward(size_t& i, Ns& dt, const point& from, const point& dest);

    Vector2ui circumscribed_bbox_size() const;

    int32_t depth_offset() const override;

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
    float anim_speed = 1;
    uint32_t anim_progress = 0;
    /// Written when the walk script ends. none while it runs, or when it was interrupted.
    walk_result last_walk = walk_result::none;

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
