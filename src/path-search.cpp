#include "path-search.hpp"
#include "global-coords.hpp"
#include "world.hpp"
#include "pass-mode.hpp"
#include "RTree-search.hpp"
#include "compat/function2.hpp"
#include <Corrade/Containers/PairStl.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Range.h>

namespace floormat {

namespace {

constexpr auto div = path_search::div_factor;
constexpr int div_BITS = 2;
static_assert(1 << div_BITS == div);

constexpr auto never_continue_1 = [](collision_data) constexpr { return path_search_continue::blocked; };
constexpr auto never_continue_ = path_search::pred{never_continue_1};
constexpr auto always_continue_1 = [](collision_data) constexpr { return path_search_continue::pass; };
constexpr auto always_continue_ = path_search::pred{always_continue_1};

} // namespace

auto path_search::never_continue() noexcept -> const pred& { return never_continue_; }
auto path_search::always_continue() noexcept -> const pred& { return always_continue_; }

bool path_search::is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p)
{
    auto& rt = *c.rtree();
    bool is_passable = true;
    rt.Search(min.data(), max.data(), [&](uint64_t data, const auto&) {
        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
        if (x.data != own_id)
        {
            if (x.pass != (uint64_t)pass_mode::pass && p(x) != path_search_continue::pass)
            {
                is_passable = false;
                //[[maybe_unused]] auto obj = c.world().find_object(x.data);
                return false;
            }
        }
        return true;
    });
    return is_passable;
}

bool path_search::is_passable_(chunk* c0, const std::array<world::neighbor_pair, 8>& neighbors,
                               Vector2 min, Vector2 max, object_id own_id, const pred& p)
{
    fm_debug_assert(max >= min);

    if (c0)
        // it's not correct to return true if c == nullptr
        // because neighbors can still contain bounding boxes for that tile
        if (!is_passable_1(*c0, min, max, own_id, p))
            return false;

    for (auto i = 0uz; i < 8; i++)
    {
        auto nb = world::neighbor_offsets[i];
        auto* c2 = neighbors[i].c;

        if (c2)
        {
            static_assert(std::size(world::neighbor_offsets) == 8);
            constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
            constexpr auto bbox_size = Vector2i(1 << sizeof(Vector2b::Type)*8);
            constexpr auto chunk_max = chunk_size + bbox_size;

            const auto off = Vector2(nb)*Vector2(chunk_size);
            const auto min_ = min - off, max_ = max - off;

            if (min_.x() > chunk_max.x() || min_.y() > chunk_max.y())
                continue;
            if (max_.x() < -bbox_size.x() || max_.y() < -bbox_size.y())
                continue;

            if (!is_passable_1(*c2, min_, max_, own_id, p))
                return false;
        }
    }

    return true;
}

bool path_search::is_passable(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id, const pred& p)
{
    auto* c = w.at(ch0);
    auto neighbors = w.neighbors(ch0);
    return is_passable_(c, neighbors, min, max, own_id, p);
}

bool path_search::is_passable(world& w, global_coords coord, Vector2b offset, Vector2ui size_,
                              object_id own_id, const pred& p)
{
    auto center = iTILE_SIZE2 * Vector2i(coord.local()) + Vector2i(offset);
    auto size = Vector2(size_);
    auto min = Vector2(center) - size*.5f, max = min + size;
    return is_passable(w, coord, min, max, own_id, p);
}

bool path_search::is_passable(world& w, global_coords coord, Vector2b offset, Vector2ub size,
                              object_id own_id, const pred& p)
{
    return is_passable(w, coord, offset, Vector2ui(size), own_id, p);
}

bool path_search::is_passable(world& w, chunk_coords_ ch, const bbox<float>& bb, object_id own_id, const pred& p)
{
    return is_passable(w, ch, bb.min, bb.max, own_id, p);
}

} // namespace floormat
