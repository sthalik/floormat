#pragma once
#include "compat/integer-types.hpp"
#include <compare>
#include <memory>

namespace floormat {

struct tile_atlas;

struct tile_image final
{
    std::shared_ptr<tile_atlas> atlas;
    std::uint16_t variant = (std::uint16_t)-1;

    explicit operator bool() const noexcept { return !!atlas; }

    std::strong_ordering operator<=>(const tile_image& o) const noexcept
    {
        const auto ret = atlas.get() <=> o.atlas.get();
        return ret != std::strong_ordering::equal ? ret : variant <=> o.variant;
    }
};

} // namespace floormat
