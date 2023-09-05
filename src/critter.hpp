#pragma once
#include "src/global-coords.hpp"
#include "src/rotation.hpp"
#include "src/object.hpp"
#include <Corrade/Containers/String.h>

namespace floormat {

struct anim_atlas;
struct world;

struct critter_proto : object_proto
{
    String name;
    bool playable : 1 = false;

    critter_proto();
    critter_proto(const critter_proto&);
    ~critter_proto() noexcept override;
    critter_proto& operator=(const critter_proto&);
    bool operator==(const object_proto& proto) const override;
};

struct critter final : object
{
    object_type type() const noexcept override;
    explicit operator critter_proto() const;

    void update(size_t i, float dt) override;
    void set_keys(bool L, bool R, bool U, bool D);
    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;

    String name;
    Vector2us offset_frac;
    bool b_L : 1 = false, b_R : 1 = false, b_U : 1 = false, b_D : 1 = false;
    bool playable : 1 = false;

private:
    int allocate_frame_time(float dt);
    static Vector2 move_vec(Vector2i vec);

    friend struct world;
    critter(object_id id, struct chunk& c, const critter_proto& proto);
};

template<> struct object_type_<struct critter> : std::integral_constant<object_type, object_type::critter> {};
template<> struct object_type_<struct critter_proto> : std::integral_constant<object_type, object_type::critter> {};

} // namespace floormat
