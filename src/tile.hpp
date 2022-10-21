#pragma once
#include "compat/defs.hpp"
#include "compat/assert.hpp"
#include "tile-defs.hpp"
#include <cstdint>
#include <memory>

namespace floormat {

struct tile_atlas;

struct tile_image final
{
    std::shared_ptr<tile_atlas> atlas;
    std::size_t variant = (std::size_t)-1;

    explicit operator bool() const noexcept { return !!atlas; }

    std::strong_ordering operator<=>(const tile_image& o) const noexcept
    {
        const auto ret = atlas.get() <=> o.atlas.get();
        return ret != std::strong_ordering::equal ? ret : variant <=> o.variant;
    }
};

struct tile final
{
    enum class pass_mode : std::uint8_t { pass_blocked, pass_ok, pass_shoot_through, };
    using enum pass_mode;

    tile_image ground_image, wall_north, wall_west;
    pass_mode passability = pass_shoot_through;

    constexpr tile() = default;
    tile(tile&&) = default;

    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(tile);
};

} //namespace floormat
