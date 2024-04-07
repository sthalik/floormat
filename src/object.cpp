#include "object.hpp"
#include "tile-constants.hpp"
#include "world.hpp"
#include "rotation.inl"
#include "anim-atlas.hpp"
#include "src/RTree-search.hpp"
#include "src/timer.hpp"
#include "compat/debug.hpp"
#include "compat/exception.hpp"
#include "compat/limits.hpp"
#include "nanosecond.inl"
#include <cmath>
#include <algorithm>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/StructuredBindings.h>
#include <Corrade/Containers/Pair.h>
#include <Magnum/Math/Functions.h>

namespace floormat {

namespace {

constexpr auto object_id_lessp = [](const auto& a, const auto& b) { return a->id < b->id; };

// todo rewrite using bitwise ops. try this instead: x = 31; int((x+64+32)/64), (x + 64 + 32)%64 - 1
template<int tile_size>
constexpr inline Pair<int, int8_t> normalize_coord(const int8_t cur, const int new_off)
{
    constexpr int8_t half_tile = tile_size/2;
    const int tmp = cur + new_off;
    auto x = (int8_t)(tmp % tile_size);
    auto t = tmp / tile_size;
    auto a = Math::abs(x);
    auto s = Math::sign(x);
    bool b = x >= half_tile | x < -half_tile;
    auto tmask = -(int)b;
    auto xmask = (int8_t)-(int8_t)b;
    t += s & tmask;
    x = (int8_t)((tile_size - a)*-s) & xmask | (int8_t)(x & ~xmask);
    return { t, x };
}

} // namespace

bool object_proto::operator==(const object_proto&) const = default;
object_proto& object_proto::operator=(const object_proto&) = default;
object_proto::~object_proto() noexcept = default;
object_proto::object_proto() = default;
object_proto::object_proto(const object_proto&) = default;
object_type object_proto::type_of() const noexcept { return type; }

object::object(object_id id, class chunk& c, const object_proto& proto) :
    id{id}, c{&c}, atlas{proto.atlas},
    offset{proto.offset}, bbox_offset{proto.bbox_offset},
    bbox_size{proto.bbox_size}, delta{proto.delta},
    frame{proto.frame}, r{proto.r}, pass{proto.pass}
{
    if (id != 0)
    {
        fm_soft_assert(atlas);
        fm_soft_assert(atlas->check_rotation(r));
        fm_soft_assert(frame < atlas->info().nframes);
    }
}

object::~object() noexcept
{
    fm_debug_assert(id);
    if (c->_teardown || c->_world->_teardown) [[unlikely]]
        return;
    if (chunk::bbox bb; c->_bbox_for_scenery(*this, bb))
        c->_remove_bbox(bb);
    c->_world->do_kill_object(id);
    const_cast<object_id&>(id) = 0;
}

float object::ordinal() const
{
    return ordinal(coord.local(), offset, atlas->group(r).z_offset);
}

float object::ordinal(local_coords xy, Vector2b offset, Vector2s z_offset) const
{
    constexpr auto inv_tile_size = 1.f/TILE_SIZE2;
    auto offset_ = ordinal_offset(offset);
    auto vec = Vector2(xy) + offset_*inv_tile_size;
    return vec[0] + vec[1] + Vector2(z_offset).sum();
}

class chunk& object::chunk() const
{
    return *c;
}

size_t object::index() const
{
    auto& c = chunk();
    const auto fn = [id = id](const auto& a, const auto&) { return a->id < id; };
    auto& es = c._objects;
    auto it = std::lower_bound(es.cbegin(), es.cend(), nullptr, fn);
    fm_assert(it != es.cend());
    fm_assert((*it)->id == id);
    return (size_t)std::distance(es.cbegin(), it);
}

bool object::is_virtual() const
{
    return false;
}

point object::position() const
{
    return {coord, offset};
}

bool object::can_rotate(global_coords coord, rotation new_r, rotation old_r,
                        Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size)
{
    if (bbox_offset.isZero() && bbox_size[0] == bbox_size[1])
        return true;
    const auto offset_ = rotate_point(offset, old_r, new_r);
    const auto bbox_offset_ = rotate_point(bbox_offset, old_r, new_r);
    const auto bbox_size_ = rotate_size(bbox_size, old_r, new_r);
    return can_move_to({}, coord, offset_, bbox_offset_, bbox_size_);
}

bool object::can_rotate(rotation new_r)
{
    if (new_r == r)
        return true;

    return can_rotate(coord, new_r, r, offset, bbox_offset, bbox_size);
}

void object::rotate(size_t, rotation new_r)
{
    fm_assert(atlas->check_rotation(new_r));
    auto offset_ = !is_dynamic() ? rotate_point(offset, r, new_r) : offset;
    auto bbox_offset_ = rotate_point(bbox_offset, r, new_r);
    auto bbox_size_ = rotate_size(bbox_size, r, new_r);
    set_bbox(offset_, bbox_offset_, bbox_size_, pass);
    if (r != new_r && !is_dynamic())
        c->mark_scenery_modified();

    const_cast<rotation&>(r) = new_r;
}

point object::normalize_coords(global_coords coord, Vector2b cur, Vector2i new_off)
{
    auto [cx, ox] = normalize_coord<iTILE_SIZE2.x()>(cur.x(), new_off.x());
    auto [cy, oy] = normalize_coord<iTILE_SIZE2.y()>(cur.y(), new_off.y());
    coord += Vector2i(cx, cy);
    return { coord, { ox, oy }, };
}

point object::normalize_coords(const point& pt, Vector2i delta)
{
    return object::normalize_coords(pt.coord(), pt.offset(), delta);
}

template<bool neighbor = true>
static bool do_search(class chunk* c, chunk_coords_ coord,
                      object_id id, Vector2 min, Vector2 max, Vector2b off = {})
{
    if constexpr(neighbor)
    {
        const auto ch = chunk_coords{(int16_t)(coord.x + off[0]), (int16_t)(coord.y + off[1])};
        constexpr auto size = TILE_SIZE2 * TILE_MAX_DIM, grace = TILE_SIZE2 * 4;
        const auto off_ = Vector2(off) * size;
        min -= off_;
        max -= off_;
        if (!(min + grace >= Vector2{} && max - grace <= size)) [[likely]]
            return true;
        auto& w = c->world();
        c = w.at({ch, coord.z});
        if (!c) [[unlikely]]
            return true;
    }
    bool ret = true;
    c->rtree()->Search(min.data(), max.data(), [&](object_id data, const auto& r) {
        auto x = std::bit_cast<collision_data>(data);
        if (x.data != id && x.pass != (uint64_t)pass_mode::pass &&
            rect_intersects(min, max, {r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}))
            return ret = false;
        else
            return true;
    });
    return ret;
}

bool object::can_move_to(Vector2i delta, global_coords coord2, Vector2b offset,
                         Vector2b bbox_offset, Vector2ub bbox_size)
{
    auto [coord_, offset_] = normalize_coords(coord2, offset, delta);

    if (coord_.z() != coord.z()) [[unlikely]]
        return false;

    auto& w = *c->_world;
    auto& cʹ = coord_.chunk() == coord.chunk() ? *c : w[coord_.chunk3()];

    const auto center = Vector2(coord_.local())*TILE_SIZE2 + Vector2(offset_) + Vector2(bbox_offset),
               half_bbox = Vector2(bbox_size)*.5f,
               min = center - half_bbox, max = min + Vector2(bbox_size);
    if (!do_search<false>(&cʹ, coord_, id, min, max))
        return false;
    for (const auto& off : world::neighbor_offsets)
        if (!do_search(&cʹ, coord_, id, min, max, off))
            return false;
    return true;
}

bool object::can_move_to(Vector2i delta)
{
    return can_move_to(delta, coord, offset, bbox_offset, bbox_size);
}

void object::teleport_to(size_t& i, point pt, rotation new_r)
{
    return teleport_to(i, pt.coord(), pt.offset(), new_r);
}

void object::teleport_to(size_t& i, global_coords coord_, Vector2b offset_, rotation new_r)
{
    if (new_r == rotation_COUNT)
        new_r = r;
    else if (!atlas->check_rotation(new_r))
    {
        const auto& info = atlas->info();
        const auto *obj = info.object_name.data(), *anim = info.anim_name.data();
        fm_abort("wrong rotation %d for %s/%s!", (int)new_r, obj, anim);
    }

    fm_assert(i < c->_objects.size());
    const auto eʹ = c->_objects[i];
    fm_assert(&*eʹ == this);

    if (coord_ == coord && offset_ == offset)
        return;

    if (!is_dynamic())
        c->mark_scenery_modified();

    chunk::bbox bb0, bb1;
    const auto bb_offset = rotate_point(bbox_offset, r, new_r);
    const auto bb_size   = rotate_size(bbox_size, r, new_r);
    bool b0 = c->_bbox_for_scenery(*this, bb0),
         b1 = c->_bbox_for_scenery(*this, coord_.local(), offset_, bb_offset, bb_size, bb1);

    if (coord_.chunk() == coord.chunk())
    {
        c->_replace_bbox(bb0, bb1, b0, b1);
        const_cast<global_coords&>(coord) = coord_;
        set_bbox_(offset_, bb_offset, bb_size, pass);
        const_cast<rotation&>(r) = new_r;
    }
    else
    {
        auto& w = *c->_world;

        auto& c2 = w[coord_.chunk3()];
        if (!is_dynamic())
            c2.mark_scenery_modified();
        c2._add_bbox(bb1);
        c->remove_object(i);
        auto& es = c2._objects;
        const_cast<global_coords&>(coord) = coord_;
        set_bbox_(offset_, bb_offset, bb_size, pass);
        const_cast<rotation&>(r) = new_r;
        const_cast<class chunk*&>(c) = &c2;
        i = (size_t)std::distance(es.cbegin(), std::lower_bound(es.cbegin(), es.cend(), eʹ, object_id_lessp));
        arrayInsert(es, i, move(eʹ));
    }
}

bool object::move_to(size_t& i, Vector2i delta, rotation new_r)
{
    if (!can_rotate(new_r))
        return false;
    const auto [coord_, offset_] = normalize_coords(coord, offset, delta);
    teleport_to(i, coord_, offset_, new_r);
    return true;
}

bool object::move_to(Magnum::Vector2i delta)
{
    auto i = index();
    return move_to(i, delta, r);
}

template<typename T> requires std::is_unsigned_v<T>
uint32_t object::alloc_frame_time(const Ns& dt, T& accum, uint32_t hz, float speed)
{
    constexpr auto ns_in_sec = Ns((int)1e9);
    constexpr auto accum_max = uint64_t{limits<T>::max};
    static_assert(Ns{accum_max} * ns_in_sec.stamp != Ns{}); // check for overflow on T

    const auto from_accum = uint64_t{accum} * ns_in_sec / accum_max;
    const auto from_dt = Ns(uint64_t(double(dt.stamp) * double(speed)));
    fm_assert(from_dt <= Ns{uint64_t{1} << 53});
    const auto ticks = from_dt + from_accum;
    const auto frame_duration = ns_in_sec / hz;
    const auto nframes = (uint32_t)(ticks / frame_duration);
    const auto rem = ticks % frame_duration;
    const auto new_accum_ = rem * accum_max / uint64_t{ns_in_sec};
    const auto new_accum = (T)Math::clamp(new_accum_, uint64_t{0}, accum_max);
    [[maybe_unused]] const auto old_accum = accum;
    accum = new_accum;

#if 0
    DBG_nospace << "alloc-frame-time: "
                << "dt:" << fraction(Time::to_milliseconds(dt)) << "ms"
                << " ticks:" << ticks
                << " frames:" << nframes
                << " acc:" << new_accum_
                << " rem:" << rem;
#endif
    fm_assert(nframes < 1 << 12);
    return nframes;
}

template uint32_t object::alloc_frame_time(const Ns& dt, uint16_t& accum, uint32_t hz, float speed);
template uint32_t object::alloc_frame_time(const Ns& dt, uint32_t& accum, uint32_t hz, float speed);

void object::set_bbox_(Vector2b offset_, Vector2b bb_offset_, Vector2ub bb_size_, pass_mode pass_)
{
    const_cast<Vector2b&>(offset) = offset_;
    const_cast<Vector2b&>(bbox_offset) = bb_offset_;
    const_cast<Vector2ub&>(bbox_size) = bb_size_;
    const_cast<pass_mode&>(pass) = pass_;
}

object::operator object_proto() const
{
    object_proto ret;
    ret.atlas = atlas;
    ret.offset = offset;
    ret.bbox_offset = bbox_offset;
    ret.bbox_size = bbox_size;
    ret.delta = delta;
    ret.frame = frame;
    ret.type = type();
    ret.r = r;
    ret.pass = pass;
    return ret;
}

void object::set_bbox(Vector2b offset_, Vector2b bb_offset_, Vector2ub bb_size_, pass_mode pass)
{
    fm_assert(Vector2ui(bb_size_).product() != 0);

    if (offset != offset_)
        if (!is_dynamic())
            c->mark_scenery_modified();

    chunk::bbox bb0, bb;
    const bool b0 = c->_bbox_for_scenery(*this, bb0);
    set_bbox_(offset_, bb_offset_, bb_size_, pass);
    const bool b = c->_bbox_for_scenery(*this, bb);
    c->_replace_bbox(bb0, bb, b0, b);
}

bool object::can_activate(size_t) const { return false; }
bool object::activate(size_t) { return false; }

bool object::is_dynamic() const
{
    return atlas->info().fps > 0;
}

object_type object::type_of() const noexcept
{
    return type();
}

} // namespace floormat
