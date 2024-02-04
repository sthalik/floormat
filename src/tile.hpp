#pragma once
#include "tile-image.hpp"

namespace floormat {

class chunk;
class anim_atlas;

struct tile_proto final
{
    std::shared_ptr<class ground_atlas> ground_atlas;
    std::shared_ptr<class wall_atlas> wall_north_atlas, wall_west_atlas;
    variant_t ground_variant = 0, wall_north_variant = 0, wall_west_variant = 0;

    tile_image_proto ground() const noexcept;
    wall_image_proto wall_north() const noexcept;
    wall_image_proto wall_west() const noexcept;

    friend bool operator==(const tile_proto& a, const tile_proto& b) noexcept;
};

struct tile_ref final
{
    tile_ref(class chunk& c, uint8_t i) noexcept;

    tile_image_ref ground() noexcept;
    wall_image_ref wall_north() noexcept;
    wall_image_ref wall_west() noexcept;

    tile_image_proto ground() const noexcept;
    wall_image_proto wall_north() const noexcept;
    wall_image_proto wall_west() const noexcept;

    std::shared_ptr<class ground_atlas> ground_atlas() noexcept;
    std::shared_ptr<class wall_atlas> wall_north_atlas() noexcept;
    std::shared_ptr<class wall_atlas> wall_west_atlas() noexcept;

    std::shared_ptr<const class ground_atlas> ground_atlas() const noexcept;
    std::shared_ptr<const class wall_atlas> wall_north_atlas() const noexcept;
    std::shared_ptr<const class wall_atlas> wall_west_atlas() const noexcept;

    explicit operator tile_proto() const noexcept;

    class chunk& chunk() noexcept { return *_chunk; }
    const class chunk& chunk() const noexcept { return *_chunk; }
    size_t index() const noexcept { return i; }

    friend bool operator==(const tile_ref& a, const tile_ref& b) noexcept;

private:
    class chunk* _chunk;
    uint8_t i;
};

} //namespace floormat
