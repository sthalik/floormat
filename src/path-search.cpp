#include "path-search.hpp"
#include "global-coords.hpp"
#include "object.hpp"
#include "world.hpp"
#include "RTree-search.hpp"
#include <bit>
#include <algorithm>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/PairStl.h>
#include <Magnum/Math/Range.h>

namespace floormat {

search_result::~search_result() = default;

void search::ensure_allocated(chunk_coords a, chunk_coords b)
{
    auto new_size = Math::abs(a - b) + Vector2i(3);
    auto new_start = Vector2i(std::min(a.x, b.x), std::min(a.y, b.y)) - Vector2i(1);
    auto size1 = new_size.product();
    fm_debug_assert(size1 > 0);
    cache.start = new_start;
    if ((size_t)size1 > cache.array.size())
    {
        cache.array = Array<chunk_tiles_cache>{ValueInit, (size_t)size1};
        cache.size = new_size;
    }
    else
        for (auto& x : cache.array)
            x = {};
}

bool search::is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id)
{
    auto& rt = *c.rtree();
    bool is_passable = true;
    rt.Search(min.data(), max.data(), [&](uint64_t data, const auto&) {
        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
        if (x.data != own_id && x.pass != (uint64_t)pass_mode::pass)
        {
            is_passable = false;
            return false;
        }
        else
            return true;
    });
    return is_passable;
}

bool search::is_passable(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id)
{
    if (auto* c = w.at(ch0))
        // it's not correct to return true if c == nullptr
        // because neighbors can still contain bounding boxes for that tile
        if (!is_passable_1(*c, min, max, own_id))
            return false;

    for (auto nb : world::neighbor_offsets)
    {
        static_assert(iTILE_SIZE2.x() == iTILE_SIZE2.y());
        constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
        constexpr int bbox_max = 1 << sizeof(Vector2b().x())*8;
        constexpr auto chunk_max = chunk_size + Vector2i(bbox_max);

        const auto off = Vector2(nb)*Vector2(chunk_size);
        const auto min_ = min - off, max_ = max - off;

        if (min_.x() > chunk_max.x() || min_.y() > chunk_max.y())
            continue;
        if (max_.x() < -bbox_max || max_.y() < -bbox_max)
            continue;

        if (auto* c2 = w.at(ch0 + Vector2i(nb)))
            if (!is_passable_1(*c2, min_, max_, own_id))
                return false;
    }
    return true;
}

bool search::is_passable(world& w, global_coords coord, Vector2b offset, Vector2ub size_, object_id own_id)
{
    auto center = iTILE_SIZE2 * Vector2i(coord.local()) + Vector2i(offset);
    auto size = Vector2(size_);
    auto min = Vector2(center) - size*.5f, max = min + size;
    return is_passable(w, coord, min, max, own_id);
}

auto search::make_neighbor_tile_bbox(Vector2i coord, Vector2ub own_size, rotation r) -> bbox
{
    constexpr auto get_value = [](Vector2i coord, Vector2i sz, rotation r) constexpr -> Pair<Vector2i, Vector2i> {
        constexpr auto half_tile = iTILE_SIZE2/2;
        constexpr int offset_W = iTILE_SIZE2.x(), offset_N = iTILE_SIZE2.y();

        auto tile_center = coord * iTILE_SIZE2;
        auto tile_start = tile_center - half_tile;

        auto empty_space_NS = (iTILE_SIZE2.x() - sz.x()) / 2;
        auto empty_space_WE = (iTILE_SIZE2.y() - sz.y()) / 2;

        switch (r)
        {
        case rotation::N: {
            auto min_N = Vector2i(tile_start.x() + empty_space_NS,  tile_center.y() - offset_N          );
            auto max_N = Vector2i(min_N.x() + sz.x(),               tile_center.y()                     );
            return {min_N, max_N};
        }
        case rotation::S: {
            auto min_S = Vector2i(tile_start.x() + empty_space_NS,   tile_center.y()                    );
            auto max_S = Vector2i(min_S.x() + sz.x(),                tile_center.y() + offset_N         );
            return {min_S, max_S};
        }
        case rotation::W: {
            auto min_W = Vector2i(tile_center.x() - offset_W,        tile_start.y() + empty_space_WE    );
            auto max_W = Vector2i(tile_center.x(),                   min_W.y() + sz.y()                 );
            return {min_W, max_W};
            }
        case rotation::E: {
            auto min_E = Vector2i(tile_center.x(),                   tile_start.y() + empty_space_WE    );
            auto max_E = Vector2i(tile_center.x() + offset_W,        min_E.y() + sz.y()                 );
            return {min_E, max_E};
        }
        case rotation_COUNT: {
            auto min_C = Vector2i(tile_center.x() - (sz.x() >> 1), tile_center.y() - (sz.y() >> 1));
            auto max_C = min_C + sz;
            return {min_C, max_C};
        }
        default:
            fm_abort("wrong 4-way rotation enum '%d'", (int)r);
        }
    };

#if 0
    if constexpr(true)
    {
        constexpr auto sz_  = min_size;
        constexpr Vector2i coord_   = Vector2i(1, 2);

        {
            constexpr auto N = get_value(coord_, sz_, rotation::N);
            constexpr auto min_N = N.first(), max_N = N.second();
            { [[maybe_unused]] constexpr auto N_x = min_N.x(),    N_y = min_N.y();  }
            { [[maybe_unused]] constexpr auto N_x = max_N.x(),    N_y = max_N.y();  }
        }
        {
            constexpr auto E = get_value(coord_, sz_, rotation::E);
            constexpr auto min_E = E.first(), max_E = E.second();
            { [[maybe_unused]] constexpr auto E_x = min_E.x(),    E_y = min_E.y();  }
            { [[maybe_unused]] constexpr auto E_x = max_E.x(),    E_y = max_E.y();  }
        }
        {
            constexpr auto S = get_value(coord_, sz_, rotation::S);
            constexpr auto min_S = S.first(), max_S = S.second();
            { [[maybe_unused]] constexpr auto S_x = min_S.x(),    S_y = min_S.y();  }
            { [[maybe_unused]] constexpr auto S_x = max_S.x(),    S_y = max_S.y();  }
        }
        {
            constexpr auto W = get_value(coord_, sz_, rotation::W);
            constexpr auto min_W = W.first(), max_W = W.second();
            { [[maybe_unused]] constexpr auto W_x = min_W.x(),    W_y = min_W.y();  }
            { [[maybe_unused]] constexpr auto W_x = max_W.x(),    W_y = max_W.y();  }
        }
    }
#endif

    constexpr auto min_size = iTILE_SIZE2*3/4;
    static_assert(min_size.x() % 2 == 0);

    auto sz  = Math::max(Vector2i(own_size), min_size);
    auto [min, max] = get_value(coord, sz, r);
    return { Vector2(min), Vector2(max) };
}

Optional<search_result> search::operator()(world& w, object_id own_id,
                                           global_coords from, Vector2b from_offset, Vector2ub size,
                                           global_coords to, Vector2b to_offset)
{
    if (from.z() != to.z()) [[unlikely]]
        return {};
    // todo try removing this eventually
    if (from.z() != 0) [[unlikely]]
        return {};

    // check if obj can actually move at all
    if (!is_passable(w, from, from_offset, size, own_id))
        return {};

    ensure_allocated(from.chunk(), to.chunk());

    // todo...
    return {};
}

Optional<search_result> search::operator()(world& w, const object& obj, global_coords to, Vector2b to_offset)
{
    constexpr auto full_tile = Vector2ub(iTILE_SIZE2*3/4);
    auto size = Math::max(obj.bbox_size, full_tile);

    // todo fixme
    // if bbox_offset is added to obj's offset, then all coordinates in the paths are shifted by bbox_offset.
    // maybe add handling to subtract bbox_offset from the returned path.
    // for that it needs to be passed into callees separately.
    fm_assert(obj.bbox_offset.isZero());
    return operator()(w, obj.id, obj.coord, obj.offset, size, to, to_offset);
}

} // namespace floormat
