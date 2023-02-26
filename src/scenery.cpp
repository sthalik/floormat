#include "scenery.hpp"
#include "anim-atlas.hpp"
#include "compat/assert.hpp"
#include <algorithm>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/TripleStl.h>

namespace floormat {

namespace {

struct rotation_symmetry final {
    rotation r = rotation::N;
    Triple<Vector2b, Vector2ub, Vector2ub> value;
};

constexpr Pair<rotation, Triple<Vector2b, Vector2ub, Vector2ub>> rotation_symmetries[] = {
    { rotation::N, { { 1,  1}, {0, 1}, {0, 1} } },
    { rotation::E, { {-1,  1}, {1, 0}, {1, 0} } },
    { rotation::S, { {-1, -1}, {0, 1}, {0, 1} } },
    { rotation::W, { { 1, -1}, {1, 0}, {1, 0} } },
};

constexpr Triple<Vector2b, Vector2ub, Vector2ub> symmetry_for_rot(rotation r)
{
    fm_assert(r < rotation_COUNT && (std::size_t)r % 2 == 0);
    auto idx = (std::size_t)r / 2;
    const auto& [r1, sym] = rotation_symmetries[idx];
    fm_debug_assert(r1 == r);
    return sym;
}

constexpr Pair<Vector2b, Vector2ub> rotate_bbox_to(Vector2b offset0, Vector2ub size0, rotation r_old, rotation r_new)
{
    fm_assert(r_old < rotation_COUNT && (std::size_t)r_old % 2 == 0);
    fm_assert(r_new < rotation_COUNT && (std::size_t)r_new % 2 == 0);
    auto [m_offset0, i_offset0, i_size0] = symmetry_for_rot(r_old);
    auto offset0_ = offset0 * m_offset0;
    auto offset_n = Vector2b(offset0_[i_offset0[0]], offset0_[i_offset0[1]]);
    fm_debug_assert(r_old != rotation::N || offset_n == offset0);
    auto size_n = Vector2ub(size0[i_size0[0]], size0[i_size0[1]]);
    fm_debug_assert(r_old != rotation::N || size_n == size0);
    auto [m_offset1, i_offset1, i_size1] = symmetry_for_rot(r_new);
    return {
        Vector2b{offset_n[i_offset1[0]], offset_n[i_offset1[1]]}*m_offset1,
        Vector2ub{size_n[i_size1[0]], size_n[i_size1[1]]},
    };
}

constexpr Vector2b rotate_bbox_offset(Vector2b offset0, rotation r_old, rotation r_new)
{
    fm_assert(r_old < rotation_COUNT && (std::size_t)r_old % 2 == 0);
    fm_assert(r_new < rotation_COUNT && (std::size_t)r_new % 2 == 0);
    auto [m_offset0, i_offset0, i_size0] = symmetry_for_rot(r_old);
    auto offset0_ = offset0 * m_offset0;
    auto offset_n = Vector2b(offset0_[i_offset0[0]], offset0_[i_offset0[1]]);
    //auto size_n = Vector2ub(size0[i_size0[0]], size0[i_size0[1]]);
    //fm_debug_assert(r_old != rotation::N || offset_n == offset0 && size_n == size0);
    auto [m_offset1, i_offset1, i_size1] = symmetry_for_rot(r_new);
    return Vector2b{offset_n[i_offset1[0]], offset_n[i_offset1[1]]}*m_offset1;
}

constexpr Vector2ub rotate_bbox_size(Vector2ub size0, rotation r_old, rotation r_new)
{
    fm_assert(r_old < rotation_COUNT && (std::size_t)r_old % 2 == 0);
    fm_assert(r_new < rotation_COUNT && (std::size_t)r_new % 2 == 0);
    auto [m_offset0, i_offset0, i_size0] = symmetry_for_rot(r_old);
    auto size_n = Vector2ub(size0[i_size0[0]], size0[i_size0[1]]);
    //fm_debug_assert(r_old != rotation::N || offset_n == offset0 && size_n == size0);
    auto [m_offset1, i_offset1, i_size1] = symmetry_for_rot(r_new);
    return Vector2ub{size_n[i_size1[0]], size_n[i_size1[1]]};
}

constexpr Pair<Vector2b, Vector2ub> rot_for_door(rotation r)
{
    constexpr Pair<Vector2b, Vector2ub> door_north = {
        { 0, -32 }, { 32, 16 },
    };
    auto [offset, size] = door_north;
    return rotate_bbox_to(offset, size, rotation::N, r);
};

/* N   0   -32    32  16
 * E   32   0     16  32
 * S   0    32    32  16
 * W  -32   0     16  32
 */
static_assert(rot_for_door(rotation::N) == Pair<Vector2b, Vector2ub>{{ 0, -32}, {32, 16}});
static_assert(rot_for_door(rotation::E) == Pair<Vector2b, Vector2ub>{{32,  0 }, {16, 32}});
static_assert(rot_for_door(rotation::S) == Pair<Vector2b, Vector2ub>{{ 0,  32}, {32, 16}});
static_assert(rot_for_door(rotation::W) == Pair<Vector2b, Vector2ub>{{-32, 0 }, {16, 32}});

/* N   16  -32    32  16
 * E   32   16    16  32
 * S  -16   32    32  16
 * W  -32  -16    16  32
 */
static_assert(rotate_bbox_to({ 16, -32}, {32, 16}, rotation::N, rotation::E) == Pair<Vector2b, Vector2ub>{{ 32,  16}, {16, 32}});
static_assert(rotate_bbox_to({ 16, -32}, {32, 16}, rotation::N, rotation::S) == Pair<Vector2b, Vector2ub>{{-16,  32}, {32, 16}});
static_assert(rotate_bbox_to({ 16, -32}, {32, 16}, rotation::N, rotation::W) == Pair<Vector2b, Vector2ub>{{-32, -16}, {16, 32}});

static_assert(rotate_bbox_to({ 32,  16}, {16, 32}, rotation::E, rotation::S) == Pair<Vector2b, Vector2ub>{{-16,  32}, {32, 16}});
static_assert(rotate_bbox_to({ 32,  16}, {16, 32}, rotation::E, rotation::N) == Pair<Vector2b, Vector2ub>{{ 16, -32}, {32, 16}});
static_assert(rotate_bbox_to({-32, -16}, {16, 32}, rotation::W, rotation::S) == Pair<Vector2b, Vector2ub>{{-16,  32}, {32, 16}});

static_assert(rotate_bbox_to({1, 2}, {3, 4}, rotation::E, rotation::E) == Pair<Vector2b, Vector2ub>{{1, 2}, {3, 4}});
static_assert(rotate_bbox_to({1, 2}, {3, 4}, rotation::N, rotation::N) == Pair<Vector2b, Vector2ub>{{1, 2}, {3, 4}});

} // namespace

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

scenery::scenery(generic_tag_t, const anim_atlas& atlas, rotation r, frame_t frame,
                 pass_mode passability, bool active, bool interactive,
                 Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size) :
    frame{frame},
    offset{offset},
    bbox_offset{rotate_bbox_offset(bbox_offset, atlas.first_rotation(), r)},
    bbox_size{rotate_bbox_size(bbox_size, atlas.first_rotation(), r)},
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
    bbox_offset{rotate_bbox_offset(bbox_offset, atlas.first_rotation(), r)},
    bbox_size{rotate_bbox_size(bbox_size, atlas.first_rotation(), r)},
    r{r}, type{scenery_type::door},
    passability{is_open ? pass_mode::pass : pass_mode::blocked},
    interactive{true}
{
    fm_assert(r < rotation_COUNT);
    fm_assert(atlas.group(r).frames.size() >= 2);
}

Vector2b scenery::rotate_bbox_offset(Vector2b offset, rotation old_r, rotation r) { return floormat::rotate_bbox_offset(offset, old_r, r); }
Vector2ub scenery::rotate_bbox_size(Vector2ub size, rotation old_r, rotation r) { return floormat::rotate_bbox_size(size, old_r, r); }

void scenery::rotate(rotation new_r)
{
    bbox_offset = scenery::rotate_bbox_offset(bbox_offset, r, new_r);
    bbox_size = scenery::rotate_bbox_size(bbox_size, r, new_r);
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
