#include "scenery.hpp"
#include "anim-atlas.hpp"
#include "chunk.hpp"
#include "compat/assert.hpp"
#include "world.hpp"
#include "shaders/tile.hpp"
#include "src/rotation.inl"
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

bool scenery::can_activate(size_t) const
{
    return atlas && interactive;
}

bool scenery::update(size_t, float dt)
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
    case scenery_type::door: {
        fm_assert(atlas);
        auto& anim = *atlas;
        const auto hz = uint8_t(atlas->info().fps);
        const auto nframes = (int)anim.info().nframes;
        fm_debug_assert(anim.info().fps > 0 && anim.info().fps <= 0xff);

        auto delta_ = int(s.delta) + int(65535u * dt);
        delta_ = std::min(65535, delta_);
        const auto frame_time = int(1.f/hz * 65535);
        const auto n = (uint8_t)std::clamp(delta_ / frame_time, 0, 255);
        s.delta = (uint16_t)std::clamp(delta_ - frame_time*n, 0, 65535);
        fm_debug_assert(s.delta >= 0);
        if (n == 0)
            return false;
        const int8_t dir = s.closing ? 1 : -1;
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
        const auto new_frame = (uint16_t)std::clamp(fr, 0, nframes-1);
        //Debug{} << "frame" << new_frame << nframes-1;
        s.frame = new_frame;
        if (!s.active)
            s.delta = s.closing = 0;
        //if ((p == pass_mode::pass) != (old_pass == pass_mode::pass)) Debug{} << "update: need reposition" << (s.frame == 0 ? "-1" : "1");
    }
    }

    return false;
}

Vector2 scenery::ordinal_offset(Vector2b offset) const
{
    if (sc_type == scenery_type::door)
    {
        constexpr auto off_closed_ = Vector2b(0, -bTILE_SIZE[1]/2+2);
        constexpr auto off_opened_ = Vector2b(-bTILE_SIZE[0]+2, -bTILE_SIZE[1]/2+2);
        const auto off_closed = rotate_point(off_closed_, rotation::N, r);
        const auto off_opened = rotate_point(off_opened_, rotation::N, r);
        const auto vec = frame == atlas->info().nframes-1 ? off_closed : off_opened;
        return Vector2(offset) + Vector2(vec);
    }
    return Vector2(offset);
}

Vector2 scenery::depth_offset() const
{
    constexpr auto sc_offset = tile_shader::scenery_depth_offset;
    constexpr auto inv_tile_size = 1.f/TILE_SIZE2;
    Vector2 ret;
    ret += Vector2(atlas->group(r).depth_offset) * inv_tile_size;
    if (sc_type == scenery_type::door)
    {
        const bool is_open = frame != atlas->info().nframes-1;
        ret += Vector2(is_open ? sc_offset : -sc_offset, 0);
        constexpr auto off_opened = Vector2(-1, 0);
        constexpr auto off_closed = Vector2(0, 0);
        const auto vec = is_open ? off_opened : off_closed;
        const auto offset = rotate_point(vec, rotation::N, r);
        ret += offset;
    }
    else
        ret += Vector2(sc_offset, 0);

    return ret;
}

bool scenery::activate(size_t)
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

entity_type scenery::type() const noexcept { return entity_type::scenery; }

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

scenery::scenery(object_id id, struct chunk& c, const scenery_proto& proto) :
    entity{id, c, proto}, sc_type{proto.sc_type}, active{proto.active},
    closing{proto.closing}, interactive{proto.interactive}
{
}

} // namespace floormat
