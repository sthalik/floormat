#include "critter.hpp"
#include "critter-script.hpp"
#include "compat/limits.hpp"
#include "tile-constants.hpp"
#include "src/point.inl"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "shaders/shader.hpp"
#include "compat/exception.hpp"
#include <cmath>
#include <utility>
#include <array>
#include <mg/Functions.h>

namespace floormat {

namespace {

constexpr auto arrows_to_dir(bool left, bool right, bool up, bool down)
{
    constexpr unsigned L = 1 << 3, R = 1 << 2, U = 1 << 1, D = 1 << 0;
    const unsigned bits = left*L | right*R | up*U | down*D;
    constexpr unsigned mask = L|R|U|D;
    CORRADE_ASSUME((bits & mask) == bits);

    switch (bits)
    {
    using enum rotation;
    case L | U: return W;
    case L | D: return S;
    case R | U: return N;
    case R | D: return E;
    case L: return SW;
    case D: return SE;
    case R: return NE;
    case U: return NW;
    case L|(U|D): return SW;
    case R|(U|D): return NE;
    case U|(L|R): return NW;
    case D|(L|R): return SE;
    case 0:
    // degenerate case
    case L|R|D|U:
    case D|U:
    case L|R:
        return rotation{rotation_COUNT};
    }
    std::unreachable();
}
#if 0
static_assert(arrows_to_dir(true, false, false, false) == rotation::SW);
static_assert(arrows_to_dir(true, false, true, true) == rotation::SW);
static_assert(arrows_to_dir(true, false, true, false) == rotation::W);
static_assert(arrows_to_dir(false, true, false, true) == rotation::E);
static_assert(arrows_to_dir(false, false, true, false) == rotation::NW);
static_assert(arrows_to_dir(false, false, false, false) == rotation_COUNT);
static_assert(arrows_to_dir(true, true, true, true) == rotation_COUNT);
static_assert(arrows_to_dir(true, true, false, false) == rotation_COUNT);
#endif

constexpr Vector2 rotation_to_vec(rotation r)
{
    constexpr double c = critter::move_speed * critter::frame_time;
    constexpr double d = c / Vector2d{1,  1}.length();

    constexpr Vector2 array[8] = {
        Vector2(Vector2d{ 0, -1} * c),
        Vector2(Vector2d{ 1, -1} * d),
        Vector2(Vector2d{ 1,  0} * c),
        Vector2(Vector2d{ 1,  1} * d),
        Vector2(Vector2d{ 0,  1} * c),
        Vector2(Vector2d{-1,  1} * d),
        Vector2(Vector2d{-1,  0} * c),
        Vector2(Vector2d{-1, -1} * d),
    };

    CORRADE_ASSUME(r < rotation_COUNT);
    return array[(size_t)r];
}

constexpr std::array<rotation, 3> rotation_to_similar(rotation r)
{
    CORRADE_ASSUME(r < rotation_COUNT);
    switch (r)
    {
    using enum rotation;
    case N:  return {  N, NW, NE };
    case NE: return { NE,  N,  E };
    case E:  return {  E, NE, SE };
    case SE: return { SE,  E,  S };
    case S:  return {  S, SE, SW };
    case SW: return { SW,  S,  W };
    case W:  return {  W, SW, NW };
    case NW: return { NW,  W,  N };
    }
    std::unreachable();
}

template<rotation r> constexpr uint8_t get_length_axis()
{
    static_assert((int)r % 2 == 0);
    using enum rotation;
    if constexpr(r == N || r == S)
        return 1;
    else if constexpr(r == W || r == E)
        return 0;
    fm_assert(false);
}

template<rotation new_r, bool MultiStep>
CORRADE_ALWAYS_INLINE
bool update_movement_body(size_t& i, critter& C, const anim_def& info, uint8_t nsteps)
{
    constexpr auto vec = rotation_to_vec(new_r);
    using Frac = decltype(critter::offset_frac_);
    constexpr auto frac = (float{limits<Frac>::max}+1)/2;
    constexpr auto inv_frac = 1 / frac;
    const auto from_accum = C.offset_frac_ * inv_frac * vec;

    Vector2 offset_{NoInit};
    if constexpr(MultiStep)
        offset_ = vec * float(nsteps) + from_accum;
    else
        offset_ = vec + from_accum;

    auto off_i = Vector2i(offset_);
    if (!off_i.isZero())
    {
        auto rem = Math::fmod(offset_, 1.f).length();
        C.offset_frac_ = Frac(rem * frac);
        if (C.can_move_to(off_i))
        {
            C.move_to(i, off_i, new_r);
            if constexpr(MultiStep)
                (C.frame += nsteps) %= info.nframes;
            else
                ++C.frame %= info.nframes;
            return true;
        }
    }
    else
    {
        auto rem = offset_.length();
        C.offset_frac_ = Frac(rem * frac);
        return true;
    }
    return false;
}

template<rotation r>
CORRADE_ALWAYS_INLINE
bool update_movement_3way(size_t& i, critter& C, const anim_def& info)
{
    constexpr auto rotations = rotation_to_similar(r);
    if (update_movement_body<rotations[0], false>(i, C, info, 0))
        return true;
    if (update_movement_body<rotations[1], false>(i, C, info, 0))
        return true;
    if (update_movement_body<rotations[2], false>(i, C, info, 0))
        return true;
    return false;
}

constexpr bool DoUnroll = true;

template<rotation new_r>
requires (((int)new_r & 1) % 2 != 0)
CORRADE_ALWAYS_INLINE
bool update_movement_1(critter& C, size_t& i, const anim_def& info, uint32_t nframes)
{
    if constexpr(DoUnroll)
    {
        //Debug{} << "< nframes" << nframes;
        while (nframes > 1)
        {
            auto len = (uint8_t)Math::min(nframes, (uint32_t)C.bbox_size.min());
            if (len <= 1)
                break;
            if (!update_movement_body<new_r, true>(i, C, info, len))
                break;
            //Debug{} << " " << len;
            nframes -= len;
        }
        //Debug{} << ">" << nframes;
    }

    for (auto k = 0u; k < nframes; k++)
        if (!update_movement_3way<new_r>(i, C, info))
            return false;
    return true;
}

template<rotation new_r>
requires (((int)new_r & 1) % 2 == 0)
CORRADE_NEVER_INLINE
bool update_movement_1(critter& C, size_t& i, const anim_def& info, uint32_t nframes)
{
    if constexpr(DoUnroll)
    {
        //Debug{} << "< nframes" << nframes;
        while (nframes > 1)
        {
            constexpr auto len_axis = get_length_axis<new_r>();
            auto len = (uint8_t)Math::min(nframes, (uint32_t)C.bbox_size.data()[len_axis]);
            if (len <= 1) [[unlikely]]
                break;
            if (!update_movement_body<new_r, true>(i, C, info, len))
                break;
            //Debug{} << " " << len;
            nframes -= len;
        }
        //Debug{} << ">" << nframes;
    }

    for (auto k = 0u; k < nframes; k++)
        if (!update_movement_body<new_r, false>(i, C, info, 0))
            return false;
    return true;
}

template bool update_movement_1<(rotation)0>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)1>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)2>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)3>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)4>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)5>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)6>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);
template bool update_movement_1<(rotation)7>(critter& C, size_t& i, const anim_def& info, uint32_t nframes);

struct step_s
{
    uint32_t count;
    Vector2b direction;
};

constexpr step_s next_step_(Vector2i vec_in)
{
    const auto vec = Vector2ui(Math::abs(vec_in));
    const auto signs = Vector2b(Math::sign(vec_in));

    if (vec.x() == vec.y())
        return { vec.x(), Vector2b{1, 1} * signs };
    else if (vec.y() == 0)
        return { vec.x(), Vector2b{1, 0} * signs };
    else if (vec.x() == 0)
        return { vec.y(), Vector2b{0, 1} * signs };
    else
    {
        uint32_t major_idx, minor_idx;
        if (vec.x() > vec.y())
        {
            major_idx = 0;
            minor_idx = 1;
        }
        else
        {
            major_idx = 1;
            minor_idx = 0;
        }
        const auto major = vec[major_idx], minor = vec[minor_idx];
        const auto num_axis_aligned = (uint32_t)Math::abs((int)major - (int)minor);
        auto axis_aligned = Vector2b{};
        axis_aligned[major_idx] = 1;
        return { num_axis_aligned, axis_aligned * signs };
    }
}

constexpr rotation dir_from_step(step_s step)
{
    if (step.direction.isZero()) [[unlikely]]
        return rotation_COUNT;

    auto x = step.direction.x() + 1;
    auto y = step.direction.y() + 1;
    fm_debug_assert((x & 3) == x && (y & 3) == y);
    auto val = x << 2 | y;

    switch (val)
    {
    using enum rotation;
    case 0 << 2 | 0: /* -1 -1 */ return NW;
    case 0 << 2 | 1: /* -1  0 */ return W;
    case 0 << 2 | 2: /* -1  1 */ return SW;
    case 1 << 2 | 0: /*  0 -1 */ return N;
    case 1 << 2 | 1: /*  0  0 */ return rotation_COUNT;
    case 1 << 2 | 2: /*  0  1 */ return S;
    case 2 << 2 | 0: /*  1 -1 */ return NE;
    case 2 << 2 | 1: /*  1  0 */ return E;
    case 2 << 2 | 2: /*  1  1 */ return SE;
    default: return rotation_COUNT;
    }
}

constexpr step_s next_step(point from, point to)
{
    fm_debug_assert(from.chunk3().z == to.chunk3().z);
    const auto vec = to - from;
    fm_debug_assert(!vec.isZero());
    return next_step_(vec);
}

constexpr float step_magnitude(Vector2b vec)
{
    constexpr double cʹ = critter::move_speed * critter::frame_time;
    constexpr double dʹ = cʹ / Vector2d{1,  1}.length();
    constexpr auto c = (float)cʹ, d = (float)dʹ;

    if (vec.x() * vec.y() != 0)
        // diagonal
            return d;
    else
        // axis-aligned
            return c;
}

} // namespace

extern template class Script<critter, critter_script>;

critter_proto::critter_proto(const critter_proto&) = default;
critter_proto::~critter_proto() noexcept = default;
critter_proto& critter_proto::operator=(const critter_proto&) = default;

critter_proto::critter_proto()
{
    type = object_type::critter;
    atlas = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
}

bool critter_proto::operator==(const object_proto& e0) const
{
    if (type != e0.type)
        return false;

    if (!object_proto::operator==(e0))
        return false;

    const auto& s0 = static_cast<const critter_proto&>(e0);
    return name == s0.name && playable == s0.playable;
}

void critter::set_keys(bool L, bool R, bool U, bool D)
{
    movement = { L, R, U, D, movement.AUTO, false, false, false };
}

void critter::set_keys_auto()
{
    movement = { false, false, false, false, true, false, false, false };
}

float critter::depth_offset() const
{
    return tile_shader::character_depth_offset;
}

Vector2 critter::ordinal_offset(Vector2b offset) const
{
    (void)offset;
    return Vector2(offset);
}

void critter::update(const std::shared_ptr<object>& ptrʹ, size_t& i, const Ns& dt)
{
    fm_debug_assert(&*ptrʹ == this);

    check_script_update_1(script.state());
    Debug{} << "i before" << i;
    //script->on_update(std::static_pointer_cast<critter>(ptrʹ), i, dt);
    Debug{} << "i after" << i;
#if 0 // for now, objects can't delete themselves
    if (check_script_update_2(script.state())) [[unlikely]]
        return;
#endif
    if (playable && !movement.AUTO) [[unlikely]]
    {
        movement.AUTO &= (movement.L | movement.R | movement.U | movement.D) == 0;

        if (!movement.AUTO)
        {
            const auto new_r = arrows_to_dir(movement.L, movement.R, movement.U, movement.D);
            if (new_r == rotation_COUNT)
            {
                offset_frac_ = {};
                delta = 0;
            }
            else
                update_movement(i, dt, new_r);
        }
    }
}

void critter::update_movement(size_t& i, const Ns& dt, rotation new_r)
{
    const auto& info = atlas->info();
    const auto nframes = alloc_frame_time(dt, delta, info.fps, speed);
    if (nframes == 0)
        return;

    fm_assert(new_r < rotation_COUNT);
    fm_assert(is_dynamic());

    if (r != new_r)
        rotate(i, new_r);
    c->ensure_passability();

    bool ret;

    switch (new_r)
    {
    default: std::unreachable();
    case (rotation)0: ret = update_movement_1<(rotation)0>(*this, i, info, nframes); break;
    case (rotation)1: ret = update_movement_1<(rotation)1>(*this, i, info, nframes); break;
    case (rotation)2: ret = update_movement_1<(rotation)2>(*this, i, info, nframes); break;
    case (rotation)3: ret = update_movement_1<(rotation)3>(*this, i, info, nframes); break;
    case (rotation)4: ret = update_movement_1<(rotation)4>(*this, i, info, nframes); break;
    case (rotation)5: ret = update_movement_1<(rotation)5>(*this, i, info, nframes); break;
    case (rotation)6: ret = update_movement_1<(rotation)6>(*this, i, info, nframes); break;
    case (rotation)7: ret = update_movement_1<(rotation)7>(*this, i, info, nframes); break;
    }

    if (!ret) [[unlikely]]
    {
        delta = {};
        offset_frac_ = {};
    }
}

auto critter::move_toward(size_t& index, const Ns& dt, const point& dest) -> move_result
{
    fm_assert(is_dynamic());

    if (movement.L | movement.R | movement.U | movement.D) [[unlikely]]
        return { .blocked = true, .moved = false, };

    const auto& info = atlas->info();
    const auto nframes = alloc_frame_time(dt, delta, info.fps, speed);
    bool moved = false;

    set_keys_auto();

    if (nframes == 0)
        return { .blocked = false, .moved = moved };

    bool ok = true;

    for (uint32_t i = 0; i < nframes; i++)
    {
        chunk().ensure_passability();

        const auto from = position();
        if (from == dest)
        {
            //Debug{} << "done!" << from;
            //C.set_keys(false, false, false, false);
            return { .blocked = false, .moved = moved, };
        }
        const auto step = next_step(from, dest);
        //Debug{} << "step" << step.direction << step.count << "|" << C.position();
        if (step.direction == Vector2b{}) [[unlikely]]
        {
            {
                static bool once = false;
                if (!once) [[unlikely]]
                {
                    once = true;
                    DBG_nospace << "critter::move_toward: no dir for"
                                << " vec:" << (dest - from)
                                << " dest:" << dest
                                << " from:" << from;
                }
            }
            ok = false;
            break;
        }
        fm_assert(step.count > 0);
        const auto new_r = dir_from_step(step);
        using Frac = decltype(critter::offset_frac_);
        constexpr auto frac = (float{limits<Frac>::max}+1)/2;
        constexpr auto inv_frac = 1 / frac;
        const auto mag = step_magnitude(step.direction);
        const auto vec = Vector2(step.direction) * mag;
        const auto from_accum = offset_frac_ * inv_frac * vec;
        auto offset_ = vec + from_accum;
        auto off_i = Vector2i(offset_);
        //Debug{} << "vec" << vec << "mag" << mag << "off_i" << off_i << "offset_" << C.offset_frac_;

        if (!off_i.isZero())
        {
            auto rem = Math::fmod(offset_, 1.f).length();
            offset_frac_ = Frac(rem * frac);
            //Debug{} << "foo1" << C.offset_frac_;
            if (can_move_to(off_i))
            {
                move_to(index, off_i, new_r);
                moved = true;
                ++frame %= info.nframes;
            }
            else
            {
                ok = false;
                break;
            }
        }
        else
        {
            auto rem = offset_.length();
            offset_frac_ = Frac(rem * frac);
        }
    }

    if (!ok) [[unlikely]]
    {
        //Debug{} << "bad";
        set_keys(false, false, false, false);
        delta = {};
        offset_frac_ = {};
        return { .blocked = true, .moved = moved };
    }

    return { .blocked = false, .moved = moved };
}

object_type critter::type() const noexcept { return object_type::critter; }

critter::operator critter_proto() const
{
    critter_proto ret;
    static_cast<object_proto&>(ret) = object::operator object_proto();
    ret.name = name;
    ret.playable = playable;
    return ret;
}

critter::critter(object_id id, class chunk& c, critter_proto proto) :
    object{id, c, proto},
    name{move(proto.name)},
    speed{proto.speed},
    playable{proto.playable}
{
    if (!name)
        name = "(Unnamed)"_s;
    fm_soft_assert(atlas->check_rotation(r));
    fm_soft_assert(speed >= 0);
    object::set_bbox_(offset, bbox_offset, Vector2ub(iTILE_SIZE2/2), pass);
}

critter::~critter() noexcept
{
}

void critter::init_script(const std::shared_ptr<object>& ptrʹ)
{
    script.do_initialize(std::static_pointer_cast<critter>(ptrʹ));
}

void critter::destroy_script_pre(const std::shared_ptr<object>& ptrʹ, script_destroy_reason r)
{
    script.do_destroy_pre(std::static_pointer_cast<critter>(ptrʹ), r);
}

void critter::destroy_script_post()
{
    script.do_finish_destroy();
}

} // namespace floormat
