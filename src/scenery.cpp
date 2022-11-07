#include "scenery.hpp"

namespace floormat {

scenery_proto::operator bool() const noexcept { return atlas != nullptr; }

scenery_ref::scenery_ref(std::shared_ptr<anim_atlas>& atlas, scenery& frame) noexcept : atlas{atlas}, frame{frame} {}
scenery_ref::scenery_ref(const scenery_ref&) noexcept = default;
scenery_ref::scenery_ref(scenery_ref&&) noexcept = default;

scenery_ref& scenery_ref::operator=(const scenery_proto& proto) noexcept
{
    atlas = proto.atlas;
    frame = proto.frame;
    return *this;
}

scenery_ref::operator scenery_proto() const noexcept { return { atlas, frame }; }
scenery_ref::operator bool() const noexcept { return atlas != nullptr; };

} // namespace floormat
