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

void scenery_ref::rotate(rotation new_r)
{
    c->with_scenery_bbox_update(idx, [&] {
        auto& s = frame;
        s.bbox_offset = rotate_point(s.bbox_offset, s.r, new_r);
        s.bbox_size = rotate_size(s.bbox_size, s.r, new_r);
        s.r = new_r;
    });
}

bool scenery_ref::can_activate() const noexcept
{
    return frame.interactive;
}

void scenery_ref::update(float dt)
{
    auto& s = frame;
    if (!s.active)
        return;

    switch (s.type)
    {
    default:
    case scenery_type::none:
    case scenery_type::generic:
        break;
    case scenery_type::door:
        fm_assert(atlas);
        auto& anim = *atlas;
        const auto hz = std::uint8_t(atlas->info().fps);
        const auto nframes = (int)anim.info().nframes;
        fm_debug_assert(anim.info().fps > 0 && anim.info().fps <= 0xff);

        auto delta_ = int(s.delta) + int(65535u * dt);
        delta_ = std::min(65535, delta_);
        const auto frame_time = int(1.f/hz * 65535);
        const auto n = (std::uint8_t)std::clamp(delta_ / frame_time, 0, 255);
        s.delta = (std::uint16_t)std::clamp(delta_ - frame_time*n, 0, 65535);
        fm_debug_assert(s.delta >= 0);
        const std::int8_t dir = s.closing ? 1 : -1;
        const int fr = s.frame + dir*n;
        s.active = fr > 0 && fr < nframes-1;
        if (fr <= 0)
            s.passability = pass_mode::pass;
        else if (fr >= nframes-1)
            s.passability = pass_mode::blocked;
        else
            s.passability = pass_mode::see_through;
        s.frame = (scenery::frame_t)std::clamp(fr, 0, nframes-1);
        if (!s.active)
            s.delta = s.closing = 0;
        break;
    }
}

bool scenery_ref::activate()
{
    auto& s = frame;
    if (!*this || s.active)
        return false;

    switch (s.type)
    {
    default:
    case scenery_type::none:
    case scenery_type::generic:
        break;
    case scenery_type::door:
        fm_assert(s.frame == 0 || s.frame == atlas->info().nframes-1);
        s.closing = s.frame == 0;
        s.frame += s.closing ? 1 : -1;
        s.active = true;
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
