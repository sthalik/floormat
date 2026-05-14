#pragma once
#include "tile-image.hpp"
#include "compat/borrowed-ptr.hpp"

namespace floormat {

class chunk;
class anim_atlas;
template<typename Chunk> struct tile_ref_;
template<typename Chunk> bool operator==(const tile_ref_<Chunk>& a, const tile_ref_<Chunk>& b) noexcept;

struct tile_proto final
{
    bptr<class ground_atlas> ground_atlas;
    bptr<class wall_atlas> wall_north_atlas, wall_west_atlas;
    variant_t ground_variant = 0, wall_north_variant = 0, wall_west_variant = 0;

    tile_image_proto ground() const noexcept;
    wall_image_proto wall_north() const noexcept;
    wall_image_proto wall_west() const noexcept;

    friend bool operator==(const tile_proto& a, const tile_proto& b) noexcept;
};

template<typename Chunk>
struct tile_ref_ final
{
    tile_ref_(Chunk& c, uint8_t i) noexcept;

    tile_image_ref ground() noexcept requires(!std::is_const_v<Chunk>);
    wall_image_ref wall_north() noexcept requires(!std::is_const_v<Chunk>);
    wall_image_ref wall_west() noexcept requires(!std::is_const_v<Chunk>);

    tile_image_proto ground() const noexcept;
    wall_image_proto wall_north() const noexcept;
    wall_image_proto wall_west() const noexcept;

    bptr<class ground_atlas> ground_atlas() const noexcept;
    bptr<class wall_atlas> wall_north_atlas() const noexcept;
    bptr<class wall_atlas> wall_west_atlas() const noexcept;

    explicit operator tile_proto() const noexcept;

    Chunk& chunk() const noexcept;
    size_t index() const noexcept;

    friend bool operator==<Chunk>(const tile_ref_& a, const tile_ref_& b) noexcept;

private:
    Chunk* _chunk;
    uint8_t i;
};

using tile_ref = tile_ref_<chunk>;
using const_tile_ref = tile_ref_<const chunk>;

extern template struct tile_ref_<chunk>;
extern template struct tile_ref_<const chunk>;

} //namespace floormat
