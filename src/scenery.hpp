#pragma once
#include "pass-mode.hpp"
#include "tile-defs.hpp"
#include <cstdint>
#include <memory>
#include <type_traits>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Magnum.h>

namespace floormat {

struct anim_atlas;

enum class rotation : std::uint8_t {
    N, NE, E, SE, S, SW, W, NW,
};

constexpr inline std::size_t rotation_BITS = 3;
constexpr inline std::size_t rotation_MASK = (1 << rotation_BITS)-1;
constexpr inline rotation rotation_COUNT = rotation{1 << rotation_BITS};

enum class scenery_type : std::uint8_t {
    none, generic, door,
};

struct scenery final
{
    struct none_tag_t final {};
    struct generic_tag_t final {};
    struct door_tag_t final {};

    static constexpr auto none    = none_tag_t{};
    static constexpr auto generic = generic_tag_t{};
    static constexpr auto door    = door_tag_t{};

    using frame_t = std::uint16_t;

    std::uint16_t delta = 0;
    frame_t frame = 0;
    Vector2b offset, bbox_size{iTILE_SIZE2/2}, bbox_offset;
    rotation     r           : 3 = rotation::N;
    scenery_type type        : 3 = scenery_type::none;
    pass_mode    passability : 2 = pass_mode::shoot_through;
    std::uint8_t active      : 1 = false;
    std::uint8_t closing     : 1 = false;
    std::uint8_t interactive : 1 = false;

    constexpr scenery() noexcept;
    constexpr scenery(none_tag_t) noexcept;
    scenery(generic_tag_t, const anim_atlas& atlas, rotation r, frame_t frame = 0,
            pass_mode passability = pass_mode::shoot_through, bool active = false, bool interactive = false);
    scenery(door_tag_t, const anim_atlas& atlas, rotation r, bool is_open = false);

    bool operator==(const scenery&) const noexcept;

    bool can_activate(const anim_atlas& anim) const noexcept;
    bool activate(const anim_atlas& atlas);
    void update(float dt, const anim_atlas& anim);
};

constexpr scenery::scenery() noexcept : scenery{scenery::none_tag_t{}} {}
constexpr scenery::scenery(none_tag_t) noexcept {}

struct scenery_proto final
{
    std::shared_ptr<anim_atlas> atlas;
    scenery frame;

    scenery_proto() noexcept;
    scenery_proto(const std::shared_ptr<anim_atlas>& atlas, const scenery& frame) noexcept;
    scenery_proto& operator=(const scenery_proto&) noexcept;
    scenery_proto(const scenery_proto&) noexcept;

    template<typename... Ts>
    scenery_proto(scenery::generic_tag_t, const std::shared_ptr<anim_atlas>& atlas, Ts&&... args) :
        atlas{atlas}, frame{scenery::generic, *atlas, std::forward<Ts>(args)...}
    {}

    template<typename... Ts>
    scenery_proto(scenery::door_tag_t, const std::shared_ptr<anim_atlas>& atlas, Ts&&... args) :
        atlas{atlas}, frame{scenery::door, *atlas, std::forward<Ts>(args)...}
    {}

    operator bool() const noexcept;
};

struct scenery_ref final {
    std::shared_ptr<anim_atlas>& atlas;
    scenery& frame;

    scenery_ref(std::shared_ptr<anim_atlas>& atlas, scenery& frame) noexcept;
    scenery_ref(const scenery_ref&) noexcept;
    scenery_ref(scenery_ref&&) noexcept;
    scenery_ref& operator=(const scenery_proto& proto) noexcept;
    operator scenery_proto() const noexcept;
    operator bool() const noexcept;
};

} // namespace floormat
