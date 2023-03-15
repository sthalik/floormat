#include "entity.hpp"
#include "world.hpp"
#include "rotation.inl"
#include "chunk.inl"
#include "anim-atlas.hpp"
#include "RTree.hpp"
#include <algorithm>

namespace floormat {

bool entity_proto::operator==(const entity_proto&) const = default;
entity_proto& entity_proto::operator=(const entity_proto&) = default;
entity_proto::~entity_proto() noexcept = default;
entity_proto::entity_proto() = default;
entity_proto::entity_proto(const entity_proto&) = default;

entity::entity(std::uint64_t id, struct chunk& c, entity_type type) noexcept :
    id{id}, c{c}, type{type}
{
}

entity::entity(std::uint64_t id, struct chunk& c, entity_type type, const entity_proto& proto) noexcept :
    id{id}, c{c}, atlas{proto.atlas},
    offset{proto.offset}, bbox_offset{proto.bbox_offset},
    bbox_size{proto.bbox_size}, delta{proto.delta},
    frame{proto.frame}, type{type}, r{proto.r}, pass{proto.pass}
{
    fm_assert(type == proto.type);
}

entity::~entity() noexcept
{
    fm_debug_assert(id);
    if (c._teardown || c._world->_teardown) [[unlikely]]
        return;
    auto& w = *c._world;
    if (chunk::bbox bb; c._bbox_for_scenery(*this, bb))
        c._remove_bbox(bb);
    auto& es =  c._entities;
    auto it = std::find_if(es.cbegin(), es.cend(), [id = id](const auto& x) { return x->id == id; });
    fm_debug_assert(it != es.cend());
    es.erase(it);
    w.do_kill_entity(id);
}

std::uint32_t entity_proto::ordinal(local_coords local) const
{
    return entity::ordinal(local, offset);
}

std::uint32_t entity::ordinal() const
{
    return ordinal(coord.local(), offset);
}

std::uint32_t entity::ordinal(local_coords xy, Vector2b offset)
{
    constexpr auto x_size = (std::uint32_t)TILE_MAX_DIM * (std::uint32_t)iTILE_SIZE[0];
    auto vec = Vector2ui(xy) * Vector2ui(iTILE_SIZE2) + Vector2ui(offset);
    return vec[1] * x_size + vec[0];
}

struct chunk& entity::chunk() const
{
    return c;
}

auto entity::iter() const -> It
{
    auto& c = chunk();
    auto& es = c._entities;
    auto it = std::lower_bound(es.cbegin(), es.cend(), nullptr, [ord = ordinal()](const auto& a, const auto&) { return a->ordinal() < ord; });
    fm_assert(it != es.cend());
    it = std::find_if(it, es.cend(), [id = id](const auto& x) { return x->id == id; });
    fm_assert(it != es.cend());
    return it;
}

bool entity::operator==(const entity_proto& o) const
{
    return atlas.get() == o.atlas.get() &&
           type == o.type && frame == o.frame && r == o.r && pass == o.pass &&
           offset == o.offset && bbox_offset == o.bbox_offset &&
           bbox_size == o.bbox_size && delta == o.delta;
}

void entity::rotate(It, rotation new_r)
{
    auto& w = *c._world;
    w[coord.chunk()].with_scenery_update(*this, [&]() {
        bbox_offset = rotate_point(bbox_offset, r, new_r);
        bbox_size = rotate_size(bbox_size, r, new_r);
        r = new_r;
    });
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

bool entity::can_move_to(Vector2i delta)
{
    auto [coord_, offset_] = normalize_coords(coord, offset, delta);
    auto& w = *c._world;
    auto& c_ = coord.chunk() == coord_.chunk() ? c : w[coord_.chunk()];

    const auto center = Vector2(coord_.local())*TILE_SIZE2 + Vector2(offset_) + Vector2(bbox_offset),
               half_bbox = Vector2(bbox_size/2),
               min = center - half_bbox, max = min + Vector2(bbox_size);

    bool ret = true;
    c_.rtree()->Search(min.data(), max.data(), [&](std::uint64_t data, const auto&) {
        auto id2 = std::bit_cast<collision_data>(data).data;
        if (id2 != id)
            return ret = false;
        else
            return true;
    });
    return ret;
}

void entity::move(It it, Vector2i delta)
{
    auto e_ = *it;
    auto& e = *e_;
    auto& c = e.c;
    auto& w = *c._world;
    auto& coord = e.coord;
    auto& offset = e.offset;
    auto& es = c._entities;
    auto [coord_, offset_] = normalize_coords(coord, offset, delta);

    if (coord_ == coord && offset_ == offset)
        return;

    if (!e.is_dynamic())
        c.mark_scenery_modified(false);

    bool same_chunk = coord_.chunk() == coord.chunk();
    chunk::bbox bb0, bb1;
    bool b0 = c._bbox_for_scenery(e, bb0);
    coord = coord_; offset = offset_;
    bool b1 = c._bbox_for_scenery(e, bb1);

    if (same_chunk)
    {
        c._replace_bbox(bb0, bb1, b0, b1);
        auto it_ = std::lower_bound(es.cbegin(), es.cend(), e_, [ord = e.ordinal()](const auto& a, const auto&) { return a->ordinal() < ord; });
        if (it_ != it)
        {
            auto pos0 = std::distance(es.cbegin(), it), pos1 = std::distance(es.cbegin(), it_);
            if (pos1 > pos0)
                pos1--;
            es.erase(it);
            [[maybe_unused]] auto size = es.size();
            es.insert(es.cbegin() + pos1, std::move(e_));
        }
    }
    else
    {
        auto& c2 = w[coord.chunk()];
        if (!e.is_dynamic())
            c2.mark_scenery_modified(false);
        c._remove_bbox(bb0);
        c2._add_bbox(bb1);
        c.remove_entity(it);
        auto it_ = std::lower_bound(c2._entities.cbegin(), c2._entities.cend(), e_,
                                    [ord = e.ordinal()](const auto& a, const auto&) { return a->ordinal() < ord; });
        c2._entities.insert(it_, std::move(e_));
    }
}

entity::operator entity_proto() const
{
    entity_proto x;
    x.atlas = atlas;
    x.offset = offset;
    x.bbox_offset = bbox_offset;
    x.bbox_size = bbox_size;
    x.delta = delta;
    x.frame = frame;
    x.type = type;
    x.r = r;
    x.pass = pass;
    return x;
}

bool entity::can_activate(It) const { return false; }
bool entity::activate(It) { return false; }

bool entity::is_dynamic() const
{
    return atlas->info().fps > 0;
}

} // namespace floormat
