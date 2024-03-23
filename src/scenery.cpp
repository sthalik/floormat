#include "scenery.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "tile-constants.hpp"
#include "anim-atlas.hpp"
#include "rotation.inl"
#include "nanosecond.hpp"
#include "world.hpp"
#include "shaders/shader.hpp"
#include <mg/Functions.h>

namespace floormat {

namespace {

template<typename... Ts> struct [[maybe_unused]] overloaded : Ts... { using Ts::operator()...; };

#if defined __GNUG__ && !defined __clang__
#pragma GCC diagnostic push // gcc doesn't support [[attributes]] on deduction guides
#pragma GCC diagnostic ignored "-Wunused"
#endif
template<typename... Ts>
#ifdef __clang__
[[maybe_unused]]
#endif
overloaded(Ts...) -> overloaded<Ts...>;
#if defined __GNUG__ && !defined __clang__
#pragma GCC diagnostic pop
#endif

template<typename T> struct proto_to_scenery_;
template<> struct proto_to_scenery_<generic_scenery_proto> { using type = generic_scenery; };
template<> struct proto_to_scenery_<door_scenery_proto> { using type = door_scenery; };
template<typename T> using proto_to_scenery = typename proto_to_scenery_<T>::type;

template<typename T> struct scenery_to_proto_;
template<> struct scenery_to_proto_<generic_scenery> { using type = generic_scenery_proto; };
template<> struct scenery_to_proto_<door_scenery> { using type = door_scenery_proto; };
template<typename T> using scenery_to_proto = typename scenery_to_proto_<T>::type;

} // namespace

scenery_proto::scenery_proto() noexcept { type = object_type::scenery; }

scenery_proto& scenery_proto::operator=(const scenery_proto&) noexcept = default;
scenery_proto::scenery_proto(const scenery_proto&) noexcept = default;
scenery_proto::~scenery_proto() noexcept = default;
scenery_proto::operator bool() const { return atlas != nullptr; }

bool generic_scenery_proto::operator==(const generic_scenery_proto& p) const = default;
enum scenery_type generic_scenery_proto::scenery_type() const { return scenery_type::generic; }

void generic_scenery::update(scenery&, size_t, Ns) {}
Vector2 generic_scenery::ordinal_offset(const scenery&, Vector2b offset) const { return Vector2(offset); }
bool generic_scenery::can_activate(const scenery&, size_t) const { return interactive; }
bool generic_scenery::activate(floormat::scenery&, size_t) { return false; }
enum scenery_type generic_scenery::scenery_type() const { return scenery_type::generic; }

enum scenery_type scenery_proto::scenery_type() const
{
    return std::visit(
        [&]<typename T>(const T& x) { return x.scenery_type(); },
        subtype
    );
}

generic_scenery::operator generic_scenery_proto() const { return { .active = active, .interactive = interactive, }; }

generic_scenery::generic_scenery(object_id, class chunk&, const generic_scenery_proto& p) :
    active{p.active}, interactive{p.interactive}
{}

bool door_scenery_proto::operator==(const door_scenery_proto& p) const = default;
enum scenery_type door_scenery_proto::scenery_type() const { return scenery_type::door; }

enum scenery_type door_scenery::scenery_type() const { return scenery_type::door; }
door_scenery::operator door_scenery_proto() const { return { .active = active, .interactive = interactive, .closing = closing, }; }

door_scenery::door_scenery(object_id, class chunk&, const door_scenery_proto& p) :
    closing{p.closing}, active{p.active}, interactive{p.interactive}
{}

void door_scenery::update(scenery& s, size_t, Ns dt)
{
    if (!s.atlas || !active)
        return;

    fm_assert(s.atlas);
    auto& anim = *s.atlas;
    const auto nframes = (int)anim.info().nframes;
    const auto n = (int)s.alloc_frame_time(dt, s.delta, s.atlas->info().fps, 1);
    if (n == 0)
        return;
    const int8_t dir = closing ? 1 : -1;
    const int fr = s.frame + dir*n;
    active = fr > 0 && fr < nframes-1;
    pass_mode p;
    if (fr <= 0)
        p = pass_mode::pass;
    else if (fr >= nframes-1)
        p = pass_mode::blocked;
    else
        p = pass_mode::see_through;
    s.set_bbox(s.offset, s.bbox_offset, s.bbox_size, p);
    const auto new_frame = (uint16_t)Math::clamp(fr, 0, nframes-1);
    //Debug{} << "frame" << new_frame << nframes-1;
    s.frame = new_frame;
    if (!active)
        s.delta = closing = 0;
    //if ((p == pass_mode::pass) != (old_pass == pass_mode::pass)) Debug{} << "update: need reposition" << (s.frame == 0 ? "-1" : "1");
}

Vector2 door_scenery::ordinal_offset(const scenery& s, Vector2b offset) const
{
    constexpr auto bTILE_SIZE = Vector2b(iTILE_SIZE2);

    constexpr auto off_closed_ = Vector2b(0, -bTILE_SIZE[1]/2+2);
    constexpr auto off_opened_ = Vector2b(-bTILE_SIZE[0]+2, -bTILE_SIZE[1]/2+2);
    const auto off_closed = rotate_point(off_closed_, rotation::N, s.r);
    const auto off_opened = rotate_point(off_opened_, rotation::N, s.r);
    const auto vec = s.frame == s.atlas->info().nframes-1 ? off_closed : off_opened;
    return Vector2(offset) + Vector2(vec);
}

bool door_scenery::can_activate(const scenery&, size_t) const { return interactive; }

bool door_scenery::activate(scenery& s, size_t)
{
    if (active)
        return false;
    fm_assert(s.frame == 0 || s.frame == s.atlas->info().nframes-1);
    closing = s.frame == 0;
    s.frame += closing ? 1 : -1;
    active = true;
    return true;
}

bool scenery::can_activate(size_t i) const
{
    if (!atlas)
        return false;

    return std::visit(
        [&]<typename T>(const T& sc) { return sc.can_activate(*this, i); },
        subtype
    );
}

void scenery::update(size_t i, const Ns& dt)
{
    return std::visit(
        [&]<typename T>(T& sc) { sc.update(*this, i, dt); },
        subtype
    );
}

Vector2 scenery::ordinal_offset(Vector2b offset) const
{
    return std::visit(
        [&]<typename T>(const T& sc) { return sc.ordinal_offset(*this, offset); },
        subtype
    );
}

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

bool scenery::activate(size_t i)
{
    return std::visit(
        [&]<typename T>(T& sc) { return sc.activate(*this, i); },
        subtype
    );
}

bool scenery_proto::operator==(const object_proto& e0) const
{
    if (type != e0.type)
        return false;

    if (!object_proto::operator==(e0))
        return false;

    const auto& sc = static_cast<const scenery_proto&>(e0);

    if (subtype.index() != sc.subtype.index())
        return false;

    return std::visit(
        [](const auto& a, const auto& b) {
            if constexpr(std::is_same_v< std::decay_t<decltype(a)>, std::decay_t<decltype(b)> >)
                return a == b;
            else
            {
                std::unreachable();
                return false;
            }
        },
        subtype, sc.subtype
    );
}

object_type scenery::type() const noexcept { return object_type::scenery; }

scenery::operator scenery_proto() const
{
    scenery_proto ret;
    static_cast<object_proto&>(ret) = object::operator object_proto();
    std::visit(
        [&]<typename T>(const T& s) { ret.subtype = scenery_to_proto<T>(s); },
        subtype
    );
    return ret;
}

enum scenery_type scenery::scenery_type() const
{
    return std::visit(
        [&]<typename T>(const T& sc) { return sc.scenery_type(); },
        subtype
    );
}

scenery_variants scenery::subtype_from_proto(object_id id, class chunk& c, const scenery_proto_variants& variant)
{
    return std::visit(
        [&]<typename T>(const T& p) {
            return scenery_variants { std::in_place_type_t<proto_to_scenery<T>>{}, id, c, p };
        },
        variant
    );
}

scenery_variants scenery::subtype_from_scenery_type(object_id id, class chunk& c, enum scenery_type type)
{
    switch (type)
    {
    case scenery_type::generic:
        return generic_scenery{id, c, {}};
    case scenery_type::door:
        return door_scenery{id, c, {}};
    case scenery_type::none:
    default:
        break;
    }
    fm_throw("invalid scenery type"_cf, (int)type);
}

scenery::scenery(object_id id, class chunk& c, const scenery_proto& proto) :
    object{id, c, proto}, subtype{ subtype_from_proto(id, c, proto.subtype) }
{
#ifndef FM_NO_DEBUG
    if (id != 0)
        fm_debug_assert(atlas); // todo add placeholder graphic
#endif
}

} // namespace floormat
