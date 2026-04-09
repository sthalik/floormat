#include "scenery.hpp"
#include "scenery-proto.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "compat/borrowed-ptr.inl"
#include "tile-constants.hpp"
#include "anim-atlas.hpp"
#include "rotation.inl"
#include "world.hpp"
#include "shaders/shader.hpp"
#include <mg/Functions.h>

namespace floormat {

template class bptr<generic_scenery>;
template class bptr<const generic_scenery>;
template class bptr<scenery>;
template class bptr<const scenery>;
template class bptr<door_scenery>;
template class bptr<const door_scenery>;

// --- scenery ---

enum object_type scenery::type() const noexcept { return object_type::scenery; } // NOLINT(*-convert-*-to-static)

int32_t scenery::depth_offset() const
{
    return atlas->group(r).depth_offset.sum(); // todo replace with an integer
}

scenery::operator scenery_proto() const
{
    scenery_proto ret;
    static_cast<object_proto&>(ret) = object::operator object_proto();
    return ret;
}

scenery::scenery(object_id id, class chunk& c, const scenery_proto& proto) :
    object{id, c, proto}
{
#ifndef FM_NO_DEBUG
    if (id != 0)
        fm_debug_assert(atlas); // todo add placeholder graphic
#endif
}

// ---------- generic_scenery ----------

void generic_scenery::update(const bptr<object>&, size_t&, const Ns&) {}
bool generic_scenery::can_activate(size_t) const { return interactive; }
bool generic_scenery::activate(size_t) { return false; }
enum scenery_type generic_scenery::scenery_type() const { return scenery_type::generic; }

generic_scenery::operator scenery_proto() const
{
    auto p = scenery::operator scenery_proto();
    p.subtype = operator generic_scenery_proto();
    return p;
}

generic_scenery::operator generic_scenery_proto() const
{
    return {
        .active = active,
        .interactive = interactive,
    };
}

generic_scenery::generic_scenery(object_id id, class chunk& c, const generic_scenery_proto& p, const scenery_proto& p0) :
    scenery{id, c, p0}, active{p.active}, interactive{p.interactive}
{}

// ---------- door_scenery ----------

enum scenery_type door_scenery::scenery_type() const { return scenery_type::door; }

void door_scenery::update(const bptr<object>&, size_t&, const Ns& dt)
{
    if (!atlas || !active)
        return;

    fm_assert(atlas);
    auto& anim = *atlas;
    const auto nframes = (int)anim.info().nframes;
    const auto n = (int)alloc_frame_time(dt, delta, atlas->info().fps, 1);
    if (n == 0)
        return;
    const int8_t dir = closing ? 1 : -1;
    const int fr = frame + dir*n;
    active = fr > 0 && fr < nframes-1;
    pass_mode p;
    if (fr <= 0)
        p = pass_mode::pass;
    else if (fr >= nframes-1)
        p = pass_mode::blocked;
    else
        p = pass_mode::see_through;
    set_bbox(offset, bbox_offset, bbox_size, p);
    const auto new_frame = (uint16_t)Math::clamp(fr, 0, nframes-1);
    //Debug{} << "frame" << new_frame << nframes-1;
    frame = new_frame;
    if (!active)
    {
        closing = false;
        delta = 0;
    }
    //if ((p == pass_mode::pass) != (old_pass == pass_mode::pass)) Debug{} << "update: need reposition" << (frame == 0 ? "-1" : "1");
}

int32_t door_scenery::depth_offset() const
{
    constexpr Vector2i off_closed{0, 0}, off_opened{tile_size_xy+1, 0}, off_swing{0, 0};
    const auto swing_frame = atlas->info().action_frame;
    const auto vecʹ =
        frame == atlas->info().nframes-1 ? off_closed : frame >= swing_frame ? off_swing : off_opened;
    const auto vec = rotate_point(vecʹ, rotation::W, r);
    return vec.sum();
}

bool door_scenery::can_activate(size_t) const { return interactive; }

bool door_scenery::activate(size_t)
{
    if (active)
        return false;
    fm_assert(frame == 0 || frame == atlas->info().nframes-1);
    closing = frame == 0;
    frame += closing ? 1 : -1;
    active = true;
    return true;
}

door_scenery::operator scenery_proto() const
{
    auto p = scenery::operator scenery_proto();
    p.subtype = operator door_scenery_proto();
    return p;
}

door_scenery::operator door_scenery_proto() const
{
    return {
        .active = active,
        .interactive = interactive,
        .closing = closing,
    };
}

door_scenery::door_scenery(object_id id, class chunk& c, const door_scenery_proto& p, const scenery_proto& p0) :
    scenery{id, c, p0}, closing{p.closing}, active{p.active}, interactive{p.interactive}
{}

} // namespace floormat
