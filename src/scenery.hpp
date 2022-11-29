#pragma once
#include <cstdint>
#include <memory>
#include <type_traits>

namespace floormat {

struct anim_atlas;

enum class rotation : std::uint8_t {
    N, NE, E, SE, S, SW, W, NW,
};

constexpr inline rotation rotation_COUNT = rotation{8};

enum class scenery_type : std::uint8_t {
    none, generic, door,
};

struct scenery final
{
    struct none_tag_t final {};
    struct generic_tag_t final {};
    struct door_tag_t final {};

    static constexpr inline auto none    = none_tag_t{};
    static constexpr inline auto generic = generic_tag_t{};
    static constexpr inline auto door    = door_tag_t{};

    using frame_t = std::uint16_t;

    float delta = 0;
    frame_t frame = 0;
    rotation     r           : 3 = rotation::N;
    scenery_type type        : 3 = scenery_type::none;
    std::uint8_t passable    : 1 = false;
    std::uint8_t blocks_view : 1 = false; // todo
    std::uint8_t active      : 1 = false;
    std::uint8_t closing     : 1 = false;
    std::uint8_t animated    : 1 = false; // todo
    std::uint8_t interactive : 1 = false;

    scenery() noexcept;
    scenery(none_tag_t) noexcept;
    scenery(generic_tag_t, const anim_atlas& atlas, rotation r, frame_t frame = 0,
            bool passable = false, bool blocks_view = false, bool animated = false,
            bool active = false, bool interactive = false);
    scenery(door_tag_t, const anim_atlas& atlas, rotation r, bool is_open = false);

    bool can_activate() const noexcept;
    bool activate(const anim_atlas& atlas);
    void update(float dt, const anim_atlas& anim);
};

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
