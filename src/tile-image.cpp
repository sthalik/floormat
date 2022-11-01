#include "tile-image.hpp"

namespace floormat {

bool operator==(const tile_image_proto& a, const tile_image_proto& b) noexcept
{
    return a.atlas == b.atlas && a.variant == b.variant;
}

tile_image_proto::operator bool() const noexcept { return atlas != nullptr; }

tile_image_ref::tile_image_ref(std::shared_ptr<tile_atlas>& atlas, std::uint16_t& variant) noexcept :
    atlas{atlas}, variant{variant}
{
}

tile_image_ref& tile_image_ref::operator=(const tile_image_proto& proto) noexcept
{
    atlas = proto.atlas;
    variant = proto.variant;
    return *this;
}

tile_image_ref::tile_image_ref(const tile_image_ref&) noexcept = default;

tile_image_ref::operator tile_image_proto() const noexcept
{
    return { atlas, variant };
}

tile_image_ref::operator bool() const noexcept { return atlas != nullptr; }

} // namespace floormat
