#pragma once
#include <cstdint>
#include <memory>

namespace floormat {

struct tile_atlas;

using variant_t = std::uint8_t;

struct tile_image_proto final
{
    std::shared_ptr<tile_atlas> atlas;
    variant_t variant = 0;

    friend bool operator==(const tile_image_proto& a, const tile_image_proto& b) noexcept;
    operator bool() const noexcept;
};

struct tile_image_ref final
{
    std::shared_ptr<tile_atlas>& atlas;
    variant_t& variant;

    tile_image_ref(std::shared_ptr<tile_atlas>& atlas, variant_t& variant) noexcept;
    tile_image_ref(const tile_image_ref&) noexcept;
    tile_image_ref(tile_image_ref&&) noexcept;
    tile_image_ref& operator=(const tile_image_proto& tile_image_proto) noexcept;
    operator tile_image_proto() const noexcept;
    operator bool() const noexcept;
};

} // namespace floormat
