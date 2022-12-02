#pragma once
#include "tile-image.hpp"
#include "scenery.hpp"
#include "pass-mode.hpp"

namespace floormat {

struct chunk;
struct anim_atlas;

struct pass_mode_ref final
{
    pass_mode_ref(chunk& c, std::uint8_t i) noexcept;
    pass_mode_ref& operator=(pass_mode x) noexcept;
    pass_mode_ref& operator=(const pass_mode_ref& x) noexcept;
    operator pass_mode() const noexcept;

private:
    chunk* _chunk;
    std::uint8_t i;
};

struct tile_proto final
{
    std::shared_ptr<tile_atlas> ground_atlas, wall_north_atlas, wall_west_atlas;
    std::shared_ptr<anim_atlas> scenery_atlas;
    variant_t ground_variant = 0, wall_north_variant = 0, wall_west_variant = 0;
    struct scenery scenery_frame;
    pass_mode passability = pass_mode{0};

    tile_image_proto ground() const noexcept;
    tile_image_proto wall_north() const noexcept;
    tile_image_proto wall_west() const noexcept;
    scenery_proto scenery() const noexcept;

    friend bool operator==(const tile_proto& a, const tile_proto& b) noexcept;
};

struct tile_ref final
{
    tile_ref(struct chunk& c, std::uint8_t i) noexcept;

    tile_image_ref ground() noexcept;
    tile_image_ref wall_north() noexcept;
    tile_image_ref wall_west() noexcept;
    scenery_ref scenery() noexcept;

    tile_image_proto ground() const noexcept;
    tile_image_proto wall_north() const noexcept;
    tile_image_proto wall_west() const noexcept;
    scenery_proto scenery() const noexcept;

    std::shared_ptr<tile_atlas> ground_atlas() noexcept;
    std::shared_ptr<tile_atlas> wall_north_atlas() noexcept;
    std::shared_ptr<tile_atlas> wall_west_atlas() noexcept;
    std::shared_ptr<anim_atlas> scenery_atlas() noexcept;

    std::shared_ptr<const tile_atlas> ground_atlas() const noexcept;
    std::shared_ptr<const tile_atlas> wall_north_atlas() const noexcept;
    std::shared_ptr<const tile_atlas> wall_west_atlas() const noexcept;
    std::shared_ptr<const anim_atlas> scenery_atlas() const noexcept;

    pass_mode_ref passability() noexcept;
    pass_mode passability() const noexcept;

    explicit operator tile_proto() const noexcept;

    struct chunk& chunk() noexcept { return *_chunk; }
    const struct chunk& chunk() const noexcept { return *_chunk; }
    std::size_t index() const noexcept { return i; }

    friend bool operator==(const tile_ref& a, const tile_ref& b) noexcept;

private:
    struct chunk* _chunk;
    std::uint8_t i;
};

} //namespace floormat
