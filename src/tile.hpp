#pragma once
#include "tile-image.hpp"

namespace floormat {

struct chunk;
class anim_atlas;

struct tile_proto final
{
    std::shared_ptr<tile_atlas> ground_atlas, wall_north_atlas, wall_west_atlas;
    variant_t ground_variant = 0, wall_north_variant = 0, wall_west_variant = 0;

    tile_image_proto ground() const noexcept;
    tile_image_proto wall_north() const noexcept;
    tile_image_proto wall_west() const noexcept;

    friend bool operator==(const tile_proto& a, const tile_proto& b) noexcept;
};

struct tile_ref final
{
    tile_ref(struct chunk& c, uint8_t i) noexcept;

    tile_image_ref ground() noexcept;
    tile_image_ref wall_north() noexcept;
    tile_image_ref wall_west() noexcept;

    tile_image_proto ground() const noexcept;
    tile_image_proto wall_north() const noexcept;
    tile_image_proto wall_west() const noexcept;

    std::shared_ptr<tile_atlas> ground_atlas() noexcept;
    std::shared_ptr<tile_atlas> wall_north_atlas() noexcept;
    std::shared_ptr<tile_atlas> wall_west_atlas() noexcept;

    std::shared_ptr<const tile_atlas> ground_atlas() const noexcept;
    std::shared_ptr<const tile_atlas> wall_north_atlas() const noexcept;
    std::shared_ptr<const tile_atlas> wall_west_atlas() const noexcept;

    explicit operator tile_proto() const noexcept;

    struct chunk& chunk() noexcept { return *_chunk; }
    const struct chunk& chunk() const noexcept { return *_chunk; }
    size_t index() const noexcept { return i; }

    friend bool operator==(const tile_ref& a, const tile_ref& b) noexcept;

private:
    struct chunk* _chunk;
    uint8_t i;
};

} //namespace floormat
