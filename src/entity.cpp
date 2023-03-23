#include "entity.hpp"
#include "world.hpp"
#include "rotation.inl"
#include "anim-atlas.hpp"
#include "RTree.hpp"
#include "compat/exception.hpp"
#include <cmath>
#include <algorithm>

namespace floormat {

namespace {

constexpr auto entity_id_lessp = [](const auto& a, const auto& b) { return a->id < b->id; };

} // namespace

bool entity_proto::operator==(const entity_proto&) const = default;
entity_proto& entity_proto::operator=(const entity_proto&) = default;
entity_proto::~entity_proto() noexcept = default;
entity_proto::entity_proto() = default;
entity_proto::entity_proto(const entity_proto&) = default;
entity_type entity_proto::type_of() const noexcept { return type; }

entity::entity(object_id id, struct chunk& c, const entity_proto& proto) :
    id{id}, c{&c}, atlas{proto.atlas},
    offset{proto.offset}, bbox_offset{proto.bbox_offset},
    bbox_size{proto.bbox_size}, delta{proto.delta},
    frame{proto.frame}, r{proto.r}, pass{proto.pass}
{
    if (atlas)
    {
        fm_soft_assert(atlas->check_rotation(r));
        fm_soft_assert(frame < atlas->info().nframes);
    }
}

entity::~entity() noexcept
{
    fm_debug_assert(id);
    if (c->_teardown || c->_world->_teardown) [[unlikely]]
        return;
    if (chunk::bbox bb; c->_bbox_for_scenery(*this, bb))
        c->_remove_bbox(bb);
    c->_world->do_kill_entity(id);
    const_cast<object_id&>(id) = 0;
}

float entity::ordinal() const
{
    return ordinal(coord.local(), offset, atlas->group(r).z_offset);
}

float entity::ordinal(local_coords xy, Vector2b offset, Vector2s z_offset) const
{
    constexpr auto inv_tile_size = 1.f/TILE_SIZE2;
    constexpr float width = TILE_MAX_DIM+1;
    auto offset_ = ordinal_offset(offset);
    auto vec = Vector2(xy) + offset_*inv_tile_size + Vector2(z_offset)*inv_tile_size;
    return vec[1]*width + vec[0];
}

struct chunk& entity::chunk() const
{
    return *c;
}

size_t entity::index() const
{
    auto& c = chunk();
    const auto fn = [id = id](const auto& a, const auto&) { return a->id < id; };
    auto& es = c._entities;
    auto it = std::lower_bound(es.cbegin(), es.cend(), nullptr, fn);
    fm_assert(it != es.cend());
    fm_assert((*it)->id == id);
    return (size_t)std::distance(es.cbegin(), it);
}

bool entity::can_rotate(global_coords coord, rotation new_r, rotation old_r, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size)
{
    if (bbox_offset.isZero() && bbox_size[0] == bbox_size[1])
        return true;
    const auto offset_ = rotate_point(offset, old_r, new_r);
    const auto bbox_offset_ = rotate_point(bbox_offset, old_r, new_r);
    const auto bbox_size_ = rotate_size(bbox_size, old_r, new_r);
    return can_move_to({}, coord, offset_, bbox_offset_, bbox_size_);
}

bool entity::can_rotate(rotation new_r)
{
    return can_rotate(coord, new_r, r, offset, bbox_offset, bbox_size);
}

void entity::rotate(size_t, rotation new_r)
{
    fm_assert(atlas->check_rotation(new_r));
    // todo normalize coords
    auto offset_ = !is_dynamic() ? rotate_point(offset, r, new_r) : offset; // todo add boolean for this condition
    auto bbox_offset_ = rotate_point(bbox_offset, r, new_r);
    auto bbox_size_ = rotate_size(bbox_size, r, new_r);
    set_bbox(offset_, bbox_offset_, bbox_size_, pass);
    if (r != new_r && !is_dynamic())
        c->mark_scenery_modified();

    const_cast<rotation&>(r) = new_r;
}

template <typename T> constexpr T sgn(T val) { return T(T(0) < val) - T(val < T(0)); }

Pair<global_coords, Vector2b> entity::normalize_coords(global_coords coord, Vector2b cur_offset, Vector2i new_offset)
{
    auto off_tmp = Vector2i(cur_offset) + new_offset;
    auto off_new = off_tmp % iTILE_SIZE2;
    constexpr auto half_tile = iTILE_SIZE2/2;
    for (auto i = 0_uz; i < 2; i++)
    {
        auto sign = sgn(off_new[i]);
        auto absval = std::abs(off_new[i]);
        if (absval > half_tile[i])
        {
            Vector2i v(0);
            v[i] = sign;
            coord += v;
            off_new[i] = (iTILE_SIZE[i] - absval)*-sign;
        }
    }
    return { coord, Vector2b(off_new) };
}

bool entity::can_move_to(Vector2i delta, global_coords coord, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size)
{
    auto [coord_, offset_] = normalize_coords(coord, offset, delta);
    auto& w = *c->_world;
    auto& c_ = coord.chunk() == coord_.chunk() ? *c : w[coord_.chunk()];

    const auto center = Vector2(coord_.local())*TILE_SIZE2 + Vector2(offset_) + Vector2(bbox_offset),
               half_bbox = Vector2(bbox_size)*.5f,
               min = center - half_bbox, max = min + Vector2(bbox_size);

    bool ret = true;
    c_.rtree()->Search(min.data(), max.data(), [&](object_id data, const auto&) {
        auto id2 = std::bit_cast<collision_data>(data).data;
        if (id2 != id)
            return ret = false;
        else
            return true;
    });
    return ret;
}

bool entity::can_move_to(Vector2i delta)
{
    return can_move_to(delta, coord, offset, bbox_offset, bbox_size);
}

bool entity::move_to(size_t& i, Vector2i delta, rotation new_r)
{
    if (!can_rotate(new_r))
        return false;

    auto& es = c->_entities;
    fm_debug_assert(i < es.size());
    auto e_ = es[i];

    fm_assert(&*e_ == this);
    auto& w = *c->_world;
    const auto [coord_, offset_] = normalize_coords(coord, offset, delta);

    if (coord_ == coord && offset_ == offset)
        return false;

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
        //for (auto i = 0_uz; const auto& x : es) fm_debug("%zu %s %f", i++, x->atlas->name().data(), x->ordinal());
        //fm_debug("insert (%hd;%hd|%hhd;%hhd) %td -> %zu | %f", coord_.chunk().x, coord_.chunk().y, coord_.local().x, coord_.local().y, pos1, es.size(), e.ordinal());
        return false;
    }
    else
    {
        //fm_debug("change-chunk (%hd;%hd|%hhd;%hhd)", coord_.chunk().x, coord_.chunk().y, coord_.local().x, coord_.local().y);
        auto& c2 = w[coord_.chunk()];
        if (!is_dynamic())
            c2.mark_scenery_modified();
        c2._add_bbox(bb1);
        c->remove_entity(i);
        auto& es = c2._entities;
        auto it = std::lower_bound(es.cbegin(), es.cend(), e_, entity_id_lessp);
        const_cast<global_coords&>(coord) = coord_;
        set_bbox_(offset_, bb_offset, bb_size, pass);
        const_cast<rotation&>(r) = new_r;
        const_cast<struct chunk*&>(c) = &c2;
        es.insert(it, std::move(e_));
        return true;
    }
}

void entity::set_bbox_(Vector2b offset_, Vector2b bbox_offset_, Vector2ub bbox_size_, pass_mode pass_)
{
    const_cast<Vector2b&>(offset) = offset_;
    const_cast<Vector2b&>(bbox_offset) = bbox_offset_;
    const_cast<Vector2ub&>(bbox_size) = bbox_size_;
    const_cast<pass_mode&>(pass) = pass_;
}

entity::operator entity_proto() const
{
    entity_proto ret;
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

void entity::set_bbox(Vector2b offset_, Vector2b bbox_offset_, Vector2ub bbox_size_, pass_mode pass)
{
    if (offset != offset_)
        if (!is_dynamic())
            c->mark_scenery_modified();

    chunk::bbox bb0, bb;
    const bool b0 = c->_bbox_for_scenery(*this, bb0);
    set_bbox_(offset_, bbox_offset_, bbox_size_, pass);
    const bool b = c->_bbox_for_scenery(*this, bb);
    c->_replace_bbox(bb0, bb, b0, b);
}

bool entity::can_activate(size_t) const { return false; }
bool entity::activate(size_t) { return false; }

bool entity::is_dynamic() const
{
    return atlas->info().fps > 0;
}

entity_type entity::type_of() const noexcept
{
    return type();
}

} // namespace floormat
