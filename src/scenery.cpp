#include "scenery.hpp"
#include "anim-atlas.hpp"
#include "compat/assert.hpp"
#include <algorithm>

namespace floormat {

scenery_proto::scenery_proto() noexcept = default;
scenery_proto::scenery_proto(const std::shared_ptr<anim_atlas>& atlas, const scenery& frame) noexcept :
    atlas{atlas}, frame{frame}
{}

scenery_proto& scenery_proto::operator=(const scenery_proto&) noexcept = default;
scenery_proto::scenery_proto(const scenery_proto&) noexcept = default;
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
scenery_ref::operator bool() const noexcept { return atlas != nullptr; }

scenery::scenery() noexcept : scenery{none_tag_t{}} {}
scenery::scenery(none_tag_t) noexcept : passable{true} {}
scenery::scenery(generic_tag_t, const anim_atlas& atlas, rotation r, frame_t frame,
                 bool passable, bool blocks_view, bool active, bool interactive) :
    frame{frame}, r{r}, type{scenery_type::generic},
    passable{passable}, blocks_view{blocks_view}, active{active},
    interactive{interactive}
{
    fm_assert(r < rotation_COUNT);
    fm_assert(frame < atlas.group(r).frames.size());
}

scenery::scenery(door_tag_t, const anim_atlas& atlas, rotation r, bool is_open) :
    frame{frame_t(is_open ? 0 : atlas.group(r).frames.size()-1)},
    r{r}, type{scenery_type::door},
    passable{is_open}, blocks_view{!is_open}, interactive{true}
{
    fm_assert(r < rotation_COUNT);
}

bool scenery::can_activate() const noexcept
{
    return interactive;
}

void scenery::update(float dt, const anim_atlas& anim)
{
    if (!active)
        return;

    switch (type)
    {
    default:
    case scenery_type::none:
    case scenery_type::generic:
        break;
    case scenery_type::door:
        const auto hz = std::uint8_t(anim.info().fps);
        const auto nframes = (int)anim.info().nframes;
        fm_debug_assert(anim.info().fps > 0 && anim.info().fps <= 0xff);

        delta += dt;
        const float frame_time = 1.f/hz;
        const auto n = int(delta / frame_time);
        delta -= frame_time * n;
        fm_debug_assert(delta >= 0);
        const std::int8_t dir = closing ? 1 : -1;
        const int fr = frame + dir*n;
        active = fr > 0 && fr < nframes-1;
        passable = fr <= 0;
        blocks_view = !passable;
        frame = (frame_t)std::clamp(fr, 0, nframes-1);
        if (!active)
            delta = 0;
        break;
    }
}

bool scenery::activate(const anim_atlas& atlas)
{
    if (active)
        return false;

    switch (type)
    {
    default:
    case scenery_type::none:
    case scenery_type::generic:
        break;
    case scenery_type::door:
        fm_assert(frame == 0 || frame == atlas.info().nframes-1);
        closing = frame == 0;
        frame += closing ? 1 : -1;
        active = true;
        return true;
    }
    return false;
}

} // namespace floormat
