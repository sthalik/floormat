#pragma once
#include "tile-atlas.hpp"
#include "hash.hpp"
#include "defs.hpp"

#include <concepts>
#include <cstddef>
#include <tuple>
#include <array>
#include <memory>
#include <unordered_map>
#include <utility>

namespace Magnum::Examples {

static constexpr Vector3 TILE_SIZE = { 50, 50, 50 };

struct tile_image final
{
    std::shared_ptr<tile_atlas> atlas;
    std::uint8_t variant = 0xff;

    explicit operator bool() const noexcept { return !!atlas; }
};

struct tile final
{
    enum class pass_mode : std::uint8_t { pass_blocked, pass_yes, pass_shoot_through, pass_obscured };
    using enum pass_mode;

    tile_image ground_image_, wall_west_, wall_north_;
    pass_mode passability_ = pass_shoot_through;

    //explicit operator bool() const noexcept { return !!ground_image_.atlas; }
};


} //namespace Magnum::Examples
