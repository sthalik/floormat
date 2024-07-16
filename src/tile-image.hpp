#pragma once
#include "tile-defs.hpp"
#include "compat/borrowed-ptr.hpp"

namespace floormat {

class ground_atlas;
class wall_atlas;

template<typename Atlas>
struct image_proto_
{
    bptr<Atlas> atlas;
    variant_t variant = 0;

    bool operator==(const image_proto_<Atlas>& b) const noexcept;
    explicit operator bool() const noexcept;
};

template<typename Atlas, typename Proto>
struct image_ref_ final
{
    bptr<Atlas>& atlas;
    variant_t& variant;

    image_ref_(bptr<Atlas>& atlas, variant_t& variant) noexcept;
    image_ref_(const image_ref_&) noexcept;
    image_ref_& operator=(const Proto& proto) noexcept;
    operator Proto() const noexcept;
    explicit operator bool() const noexcept;
};

using tile_image_proto = image_proto_<ground_atlas>;
using tile_image_ref = image_ref_<ground_atlas, tile_image_proto>;
extern template struct image_proto_<ground_atlas>;
extern template struct image_ref_<ground_atlas, tile_image_proto>;

using wall_image_proto = image_proto_<wall_atlas>;
using wall_image_ref = image_ref_<wall_atlas, wall_image_proto>;
extern template struct image_proto_<wall_atlas>;
extern template struct image_ref_<wall_atlas, wall_image_proto>;

} // namespace floormat
