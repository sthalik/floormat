#pragma once
#include <cstdint>
#include <memory>

namespace floormat {

struct anim_atlas;

enum class rotation : std::uint16_t {
    N, NE, E, SE, S, SW, W, NW,
    COUNT,
};

struct scenery final
{
    static constexpr auto NO_FRAME = (1 << 12) - 1;

    using frame_t = std::uint16_t;

    rotation r    : 4  = rotation::N;
    frame_t frame : 12 = NO_FRAME;
};

static_assert(sizeof(scenery) == sizeof(std::uint16_t));

struct scenery_proto final {
    std::shared_ptr<anim_atlas> atlas;
    scenery frame;

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
