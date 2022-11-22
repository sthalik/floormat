#pragma once
#include <cstdint>
#include <memory>

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

    static constexpr auto NO_FRAME = (std::uint16_t)-1;
    static constexpr inline auto none    = none_tag_t{};
    static constexpr inline auto generic = generic_tag_t{};
    static constexpr inline auto door    = door_tag_t{};

    using frame_t = std::uint16_t;

    float delta = 0;
    frame_t frame = NO_FRAME;
    rotation     r        : 3 = rotation::N;
    std::uint8_t passable : 1 = false;
    std::uint8_t active   : 1 = false;
    std::uint8_t closing  : 1 = true;
    scenery_type type     : 2 = scenery_type::none;

    scenery() noexcept;
    scenery(none_tag_t) noexcept;
    scenery(generic_tag_t, rotation r, const anim_atlas& atlas, frame_t frame = 0);
    scenery(door_tag_t, rotation r, const anim_atlas& atlas, bool is_open = false);
    scenery(float dt, frame_t frame, rotation r, bool passable, scenery_type type);

    bool activate(const anim_atlas& atlas);
    void update(float dt, const anim_atlas& anim);
};

struct scenery_proto final {
    std::shared_ptr<anim_atlas> atlas;
    scenery frame;

    scenery_proto() noexcept;
    explicit scenery_proto(scenery::none_tag_t) noexcept;
    scenery_proto(const std::shared_ptr<anim_atlas>& atlas, const scenery& frame);
    scenery_proto(scenery::generic_tag_t, rotation r, const std::shared_ptr<anim_atlas>& atlas, scenery::frame_t frame = 0);
    scenery_proto(scenery::door_tag_t, rotation r, const std::shared_ptr<anim_atlas>& atlas, bool is_open = false);
    scenery_proto& operator=(const scenery_proto&) noexcept;

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
