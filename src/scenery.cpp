#include "scenery.hpp"
#include "anim-atlas.hpp"
#include "chunk.hpp"
#include "compat/assert.hpp"
#include "world.hpp"
#include <algorithm>

namespace floormat {

scenery_proto::scenery_proto()
{
    type = entity_type::scenery;
}

scenery_proto& scenery_proto::operator=(const scenery_proto&) = default;
scenery_proto::scenery_proto(const scenery_proto&) = default;
scenery_proto::~scenery_proto() noexcept = default;
scenery_proto::operator bool() const { return atlas != nullptr; }

bool scenery::can_activate(std::size_t) const
{
    return atlas && interactive;
}

bool scenery::update(std::size_t, float dt)
{
    auto& s = *this;
    if (!s.active)
        return false;

    switch (s.sc_type)
    {
    default:
    case scenery_type::none:
    case scenery_type::generic:
        return false;
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
        pass_mode p;
        if (fr <= 0)
            p = pass_mode::pass;
        else if (fr >= nframes-1)
            p = pass_mode::blocked;
        else
            p = pass_mode::see_through;
        set_bbox(offset, bbox_offset, bbox_size, p);
        s.frame = (std::uint16_t)std::clamp(fr, 0, nframes-1);
        if (!s.active)
            s.delta = s.closing = 0;
        return true;
    }
}

bool scenery::activate(std::size_t)
{
    auto& s = *this;
    if (s.active)
        return false;

    switch (s.sc_type)
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

bool scenery_proto::operator==(const entity_proto& e0) const
{
    if (type != e0.type)
        return false;

    if (!entity_proto::operator==(e0))
        return false;

    const auto& s0 = static_cast<const scenery_proto&>(e0);
    return sc_type == s0.sc_type && active == s0.active &&
           closing == s0.closing && interactive == s0.interactive;
}

scenery::operator scenery_proto() const
{
    scenery_proto ret;
    static_cast<entity_proto&>(ret) = entity::operator entity_proto();
    ret.sc_type = sc_type;
    ret.active = active;
    ret.closing = closing;
    ret.interactive = interactive;
    return ret;
}

scenery::scenery(std::uint64_t id, struct chunk& c, entity_type type_, const scenery_proto& proto) :
    entity{id, c, type_, proto}, sc_type{proto.sc_type}, active{proto.active},
    closing{proto.closing}, interactive{proto.interactive}
{
    fm_assert(type == proto.type);
}

} // namespace floormat
