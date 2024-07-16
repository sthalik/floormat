#include "scenery.hpp"
#include "scenery-proto.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "tile-constants.hpp"
#include "anim-atlas.hpp"
#include "rotation.inl"
#include "world.hpp"
#include "shaders/shader.hpp"
#include <mg/Functions.h>

namespace floormat {

// --- scenery ---

enum object_type scenery::type() const noexcept { return object_type::scenery; } // NOLINT(*-convert-*-to-static)

float scenery::depth_offset() const
{
    constexpr auto inv_tile_size = 1.f/TILE_SIZE2;
    Vector2 offset;
    offset += Vector2(atlas->group(r).depth_offset) * inv_tile_size;
    float ret = 0;
    ret += offset[1]*TILE_MAX_DIM + offset[0];
    ret += tile_shader::scenery_depth_offset;

    return ret;
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
Vector2 generic_scenery::ordinal_offset(Vector2b offset) const { return Vector2(offset); }
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

Vector2 door_scenery::ordinal_offset(Vector2b offset) const
{
    constexpr auto bTILE_SIZE = Vector2b(iTILE_SIZE2);

    constexpr auto off_closed_ = Vector2b(0, -bTILE_SIZE[1]/2+2);
    constexpr auto off_opened_ = Vector2b(-bTILE_SIZE[0]+2, -bTILE_SIZE[1]/2+2);
    const auto off_closed = rotate_point(off_closed_, rotation::N, r);
    const auto off_opened = rotate_point(off_opened_, rotation::N, r);
    const auto vec = frame == atlas->info().nframes-1 ? off_closed : off_opened;
    return Vector2(offset) + Vector2(vec);
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
