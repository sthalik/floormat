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
};

bool operator==(const tile_image& a, const tile_image& b) noexcept;

} // namespace floormat
