#include "tile-image.hpp"

namespace floormat {

template<typename Atlas> bool image_proto_<Atlas>::operator==(const image_proto_<Atlas>& b) const noexcept = default;
template<typename Atlas> image_proto_<Atlas>::operator bool() const noexcept { return atlas != nullptr; }

template<typename Atlas, typename Proto>
image_ref_<Atlas, Proto>::operator bool() const noexcept
{
    return atlas != nullptr;
}

template<typename Atlas, typename Proto>
image_ref_<Atlas, Proto>::image_ref_(const image_ref_<Atlas, Proto>& o) noexcept
    : atlas{o.atlas}, variant{o.variant}
{}

template<typename Atlas, typename Proto>
image_ref_<Atlas, Proto>::image_ref_(std::shared_ptr<Atlas>& atlas, variant_t& variant) noexcept
    : atlas{atlas}, variant{variant}
{}

template<typename Atlas, typename Proto>
image_ref_<Atlas, Proto>::operator Proto() const noexcept
{
    return { atlas, variant };
}

template<typename Atlas, typename Proto>
image_ref_<Atlas, Proto>& image_ref_<Atlas, Proto>::operator=(const Proto& proto) noexcept
{
    atlas = proto.atlas;
    variant = proto.variant;
    return *this;
}

template struct image_proto_<tile_atlas>;
template struct image_ref_<tile_atlas, tile_image_proto>;

template struct image_proto_<wall_atlas>;
template struct image_ref_<wall_atlas, wall_image_proto>;

} // namespace floormat
