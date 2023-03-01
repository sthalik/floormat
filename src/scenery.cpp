#include "scenery.hpp"
#include "anim-atlas.hpp"
#include "chunk.hpp"
#include "compat/assert.hpp"
#include "rotation.inl"
#include <algorithm>

namespace floormat {

scenery_proto::scenery_proto() noexcept = default;
scenery_proto::scenery_proto(const std::shared_ptr<anim_atlas>& atlas, const scenery& frame) noexcept :
    atlas{atlas}, frame{frame}
{}

scenery_proto& scenery_proto::operator=(const scenery_proto&) noexcept = default;
scenery_proto::scenery_proto(const scenery_proto&) noexcept = default;
scenery_proto::operator bool() const noexcept { return atlas != nullptr; }

scenery_ref::scenery_ref(struct chunk& c, std::size_t i) noexcept :
      atlas{c.scenery_atlas_at(i)}, frame{c.scenery_at(i)},
      c{&c}, idx{std::uint8_t(i)}
{}
scenery_ref::scenery_ref(const scenery_ref&) noexcept = default;
scenery_ref::scenery_ref(scenery_ref&&) noexcept = default;
struct chunk& scenery_ref::chunk() noexcept { return *c; }
std::uint8_t scenery_ref::index() const noexcept { return idx; }

scenery_ref& scenery_ref::operator=(const scenery_proto& proto) noexcept
{
    atlas = proto.atlas;
    frame = proto.frame;
    return *this;
}

scenery_ref::operator scenery_proto() const noexcept { return { atlas, frame }; }
scenery_ref::operator bool() const noexcept { return atlas != nullptr; }

scenery::scenery(generic_tag_t, const anim_atlas& atlas, rotation r, frame_t frame,
                 pass_mode passability, bool active, bool interactive,
                 Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size) :
    frame{frame},
    offset{offset},
    bbox_offset{rotate_point(bbox_offset, atlas.first_rotation(), r)},
    bbox_size{rotate_size(bbox_size, atlas.first_rotation(), r)},
    r{r}, type{scenery_type::generic},
    passability{passability},
    active{active}, interactive{interactive}
{
    fm_assert(r < rotation_COUNT);
    fm_assert(frame < atlas.group(r).frames.size());
}

scenery::scenery(door_tag_t, const anim_atlas& atlas, rotation r, bool is_open,
                 Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size) :
    frame{frame_t(is_open ? 0 : atlas.group(r).frames.size()-1)},
    offset{offset},
    bbox_offset{rotate_point(bbox_offset, atlas.first_rotation(), r)},
    bbox_size{rotate_size(bbox_size, atlas.first_rotation(), r)},
    r{r}, type{scenery_type::door},
    passability{is_open ? pass_mode::pass : pass_mode::blocked},
    interactive{true}
{
    fm_assert(r < rotation_COUNT);
    fm_assert(atlas.group(r).frames.size() >= 2);
}

void scenery::rotate(rotation new_r)
{
    bbox_offset = rotate_point(bbox_offset, r, new_r);
    bbox_size = rotate_size(bbox_size, r, new_r);
    r = new_r;
}

bool scenery::can_activate(const anim_atlas&) const noexcept
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

        auto delta_ = int(delta) + int(65535u * dt);
        delta_ = std::min(65535, delta_);
        const auto frame_time = int(1.f/hz * 65535);
        const auto n = (std::uint8_t)std::clamp(delta_ / frame_time, 0, 255);
        delta = (std::uint16_t)std::clamp(delta_ - frame_time*n, 0, 65535);
        fm_debug_assert(delta >= 0);
        const std::int8_t dir = closing ? 1 : -1;
        const int fr = frame + dir*n;
        active = fr > 0 && fr < nframes-1;
        if (fr <= 0)
            passability = pass_mode::pass;
        else if (fr >= nframes-1)
            passability = pass_mode::blocked;
        else
            passability = pass_mode::see_through;
        frame = (frame_t)std::clamp(fr, 0, nframes-1);
        if (!active)
            delta = closing = 0;
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

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
bool scenery::operator==(const scenery&) const noexcept = default;
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

} // namespace floormat
