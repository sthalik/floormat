#include "chunk.hpp"
#include "tile-atlas.hpp"
#include "object.hpp"
#include "src/RTree-search.hpp"
#include "src/chunk-scenery.hpp"
#include "src/tile-bbox.hpp"
#include "src/wall-atlas.hpp"
#include <bit>
#include <Corrade/Containers/StructuredBindings.h>
#include <Corrade/Containers/Pair.h>

namespace floormat {

chunk::RTree* chunk::rtree() noexcept { ensure_passability(); return &_rtree; }

namespace {

constexpr object_id make_id(collision_type type, pass_mode p, object_id id)
{
    return std::bit_cast<object_id>(collision_data { (object_id)type, (object_id)p, id });
}

} // namespace

void chunk::ensure_passability() noexcept
{
    fm_assert(_objects_sorted); // not strictly necessary

    if (!_pass_modified)
        return;
    _pass_modified = false;

    _rtree.RemoveAll();

    for (const std::shared_ptr<object>& s : objects())
    {
        bbox box;
        if (_bbox_for_scenery(*s, box))
            _add_bbox(box);
    }

    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        if (const auto* atlas = ground_atlas_at(i))
        {
            auto [min, max] = whole_tile(i);
            auto id = make_id(collision_type::geometry, atlas->pass_mode(), i+1);
            _rtree.Insert(min.data(), max.data(), id);
        }
    }
    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        auto tile = operator[](i);
        if (const auto* atlas = tile.wall_north_atlas().get())
        {
            auto [min, max] = wall_north(i, atlas->info().depth);
            auto id = make_id(collision_type::geometry, atlas->info().passability, TILE_COUNT+i+1);
            _rtree.Insert(min.data(), max.data(), id);
        }
        if (const auto* atlas = tile.wall_west_atlas().get())
        {
            auto [min, max] = wall_west(i, atlas->info().depth);
            auto id = make_id(collision_type::geometry, atlas->info().passability, TILE_COUNT*2+i+1);
            _rtree.Insert(min.data(), max.data(), id);
        }
    }
}

bool chunk::_bbox_for_scenery(const object& s, local_coords local, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, bbox& value) noexcept
{
    auto [start, end] = scenery_tile(local, offset, bbox_offset, bbox_size);
    auto id = make_id(collision_type::scenery, s.pass, s.id);
    value = { .id = id, .start = start, .end = end };
    return s.atlas && !Vector2ui(s.bbox_size).isZero();
}

bool chunk::_bbox_for_scenery(const object& s, bbox& value) noexcept
{
    return _bbox_for_scenery(s, s.coord.local(), s.offset, s.bbox_offset, s.bbox_size, value);
}

void chunk::_remove_bbox(const bbox& x)
{
    auto start = Vector2(x.start), end = Vector2(x.end);
    _rtree.Remove(start.data(), end.data(), x.id);
}

void chunk::_add_bbox(const bbox& x)
{
    auto start = Vector2(x.start), end = Vector2(x.end);
    _rtree.Insert(start.data(), end.data(), x.id);
}

void chunk::_replace_bbox(const bbox& x0, const bbox& x1, bool b0, bool b1)
{
    if (_pass_modified)
        return;

    unsigned i = (unsigned)b1 << 1 | (unsigned)(b0 ? 1 : 0) << 0;
    CORRADE_ASSUME(i < 4u); (void)0;

    switch (i) // NOLINT(hicpp-multiway-paths-covered)
    {
    case 1 << 1 | 1 << 0:
        if (x1 == x0)
            return;
        _remove_bbox(x0);
        [[fallthrough]];
    case 1 << 1 | 0 << 0:
        _add_bbox(x1);
        return;
    case 0 << 1 | 1 << 0:
        _remove_bbox(x0);
        return;
    case 0 << 1 | 0 << 0:
        return;
    }
    std::unreachable();
}

bool chunk::can_place_object(const object_proto& proto, local_coords pos)
{
    (void)ensure_scenery_mesh();

    switch (proto.pass)
    {
    case pass_mode::blocked:
    case pass_mode::see_through:
        break;
    case pass_mode::pass:
    case pass_mode::shoot_through:
        return true;
    }

    if (!proto.bbox_size.x() || proto.bbox_size.y())
        return true;

    auto bbox_size = Vector2i(proto.bbox_size);
    if (bbox_size.x() > 1)
        bbox_size.x() -= 1;
    if (bbox_size.y() > 1)
        bbox_size.y() -= 1;

    const auto center = Vector2(pos)*TILE_SIZE2 + Vector2(proto.offset) + Vector2(proto.bbox_offset),
               min = center - Vector2(bbox_size)*.5f, max = min + Vector2(bbox_size);
    bool ret = true;
    _rtree.Search(min.data(), max.data(), [&](uint64_t data, const auto&) {
          [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
          if (x.pass == (uint64_t)pass_mode::pass || x.pass == (uint64_t)pass_mode::shoot_through)
              return true;
          return ret = false;
    });
    return ret;
}

} // namespace floormat
