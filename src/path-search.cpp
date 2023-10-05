#include "path-search.hpp"
#include "global-coords.hpp"
#include "object.hpp"
#include "world.hpp"
#include "RTree-search.hpp"
#include "compat/function2.hpp"
#include <bit>
#include <algorithm>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/PairStl.h>
#include <Magnum/Math/Range.h>

namespace floormat {

namespace {

constexpr auto div = path_search::subdivide_factor;
constexpr int div_BITS = 2;
static_assert(1 << div_BITS == div);

static_assert(path_search::min_size.x() % 2 == 0 && path_search::min_size.x() * div * 2 == iTILE_SIZE2.x());

constexpr auto never_continue_1 = [](collision_data) constexpr { return path_search_continue::blocked; };
constexpr auto never_continue_ = path_search::pred{never_continue_1};
constexpr auto always_continue_1 = [](collision_data) constexpr { return path_search_continue::pass; };
constexpr auto always_continue_ = path_search::pred{always_continue_1};

constexpr Pair<Vector2i, Vector2i> get_value(Vector2i sz, Vector2ub div, rotation r)
{
    const int offset_W = iTILE_SIZE2.x()/(int)div.x(), offset_N = iTILE_SIZE2.y()/(int)div.y();

    const auto r_ = (uint8_t)r;
    CORRADE_ASSUME(r_ <= (uint8_t)rotation_COUNT);

    switch (r_)
    {
    case (uint8_t)rotation::N: {
        auto min_N = Vector2i(-sz.x()/2,                        -offset_N - sz.y()/2            );
        auto max_N = Vector2i(min_N.x() + sz.x(),               sz.y() - sz.y()/2               );
        return {min_N, max_N};
    }
    case (uint8_t)rotation::S: {
        auto min_S = Vector2i(-sz.x()/2,                        -sz.y()                         );
        auto max_S = Vector2i(min_S.x() + sz.x(),               offset_N + sz.y() - sz.y()/2    );
        return {min_S, max_S};
    }
    case (uint8_t)rotation::W: {
        auto min_W = Vector2i(-offset_W - sz.x()/2,             -sz.y()/2                       );
        auto max_W = Vector2i(sz.x() - sz.x()/2,                min_W.y() + sz.y()              );
        return {min_W, max_W};
    }
    case (uint8_t)rotation::E: {
        auto min_E = Vector2i(-sz.x()/2,                        -sz.y()/2                       );
        auto max_E = Vector2i(offset_W + sz.x() - sz.x()/2,     min_E.y() + sz.y()              );
        return {min_E, max_E};
    }
    case (uint8_t)rotation_COUNT: {
        auto min_C = Vector2i(-(sz.x() >> 1),                   -(sz.y() >> 1)                  );
        auto max_C = min_C + sz;
        return {min_C, max_C};
    }
    default:
        fm_abort("wrong 4-way rotation enum '%d'", (int)r);
    }
};

[[maybe_unused]] constexpr bool test_offsets()
{
    constexpr auto sz_  = path_search::min_size;
    constexpr Vector2i shift = Vector2i(0, 0) * iTILE_SIZE2 + Vector2i(0, 0);

    [[maybe_unused]] constexpr auto N = get_value(sz_, {1,1}, rotation::N);
    [[maybe_unused]] constexpr auto min_N = N.first() + shift, max_N = N.second() + shift;
    [[maybe_unused]] constexpr auto N_min_x = min_N.x(), N_min_y = min_N.y();
    [[maybe_unused]] constexpr auto N_max_x = max_N.x(), N_max_y = max_N.y();

    [[maybe_unused]] constexpr auto E = get_value(sz_, {1,1}, rotation::E);
    [[maybe_unused]] constexpr auto min_E = E.first() + shift, max_E = E.second() + shift;
    [[maybe_unused]] constexpr auto E_min_x = min_E.x(), E_min_y = min_E.y();
    [[maybe_unused]] constexpr auto E_max_x = max_E.x(), E_max_y = max_E.y();

    [[maybe_unused]] constexpr auto S = get_value(sz_, {1,1}, rotation::S);
    [[maybe_unused]] constexpr auto min_S = S.first() + shift, max_S = S.second() + shift;
    [[maybe_unused]] constexpr auto S_min_x = min_S.x(), S_min_y = min_S.y();
    [[maybe_unused]] constexpr auto S_max_x = max_S.x(), S_max_y = max_S.y();

    [[maybe_unused]] constexpr auto W = get_value(sz_, {1,1}, rotation::W);
    [[maybe_unused]] constexpr auto min_W = W.first() + shift, max_W = W.second() + shift;
    [[maybe_unused]] constexpr auto W_min_x = min_W.x(), W_min_y = min_W.y();
    [[maybe_unused]] constexpr auto W_max_x = max_W.x(), W_max_y = max_W.y();

    return true;
}

static_assert(test_offsets());

[[maybe_unused]] constexpr bool test_offsets2()
{
    using enum rotation;
    constexpr auto tile_start = iTILE_SIZE2/-2;
    constexpr auto sz = Vector2i(8, 16);

    {
        constexpr auto bb = get_value(sz, Vector2ub(div), N);
        constexpr auto min = tile_start + bb.first(), max = tile_start + bb.second();
        static_assert(min.x() == -32 - sz.x()/2);
        static_assert(max.x() == -32 + sz.x()/2);
        static_assert(min.y() == -48 - sz.y()/2);
        static_assert(max.y() == -32 + sz.y()/2);
    }
    {
        constexpr auto bb = get_value(sz, Vector2ub(div), W);
        constexpr auto min = tile_start + bb.first(), max = tile_start + bb.second();
        static_assert(min.x() == -32 - 16 - sz.x()/2);
        static_assert(max.x() == -32 + sz.x()/2);
        static_assert(min.y() == -32 - sz.y()/2);
        static_assert(max.y() == -32 + sz.y()/2);
    }

    return true;
}

static_assert(test_offsets2());

struct chunk_subdiv_array { Vector2i min[div], max[div]; };

constexpr chunk_subdiv_array make_chunk_subdiv_array()
{
    return {};
}

} // namespace

path_search_result::path_search_result() = default;
auto path_search::never_continue() noexcept -> const pred& { return never_continue_; }
auto path_search::always_continue() noexcept -> const pred& { return always_continue_; }

void path_search::ensure_allocated(chunk_coords a, chunk_coords b)
{
    auto new_size = (Math::abs(a - b) + Vector2i(3));
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

    fm_assert(cache.size.product() > 0);
}

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

bool path_search::is_passable(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id, const pred& p)
{
    auto* c = w.at(ch0);
    auto neighbors = w.neighbors(ch0);
    return is_passable_(c, neighbors, min, max, own_id, p);
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

bool path_search::is_passable(world& w, global_coords coord, Vector2b offset, Vector2ub size_,
                              object_id own_id, const pred& p)
{
    auto center = iTILE_SIZE2 * Vector2i(coord.local()) + Vector2i(offset);
    auto size = Vector2(size_);
    auto min = Vector2(center) - size*.5f, max = min + size;
    return is_passable(w, coord, min, max, own_id, p);
}

auto path_search::make_neighbor_tile_bbox(Vector2i coord, Vector2ub own_size, Vector2ub div, rotation r) -> bbox<float>
{
    const auto shift = coord * iTILE_SIZE2;
    auto sz = Math::max(Vector2i(own_size), min_size);
    auto [min, max] = get_value(sz, div, r);
    return { Vector2(min + shift), Vector2(max + shift) };
}

auto path_search::get_walkable_neighbor_tiles(world& w, global_coords coord, Vector2ub size, object_id own_id, const pred& p) -> neighbors
{
    auto ch = chunk_coords_{ coord.chunk(), coord.z() };
    auto pos = Vector2i(coord.local());

#if 0
    if (auto [min, max] = make_neighbor_tile_bbox(pos, size, rotation_COUNT);
        !is_passable(w, ch, min, max, own_id))
        return {};
#endif

    neighbors ns;

    using enum rotation;
    constexpr struct {
        Vector2i off;
        rotation r = {};
    } nbs[] = {
        { {  0, -1 }, N },
        { {  1,  0 }, E },
        { {  0,  1 }, S },
        { { -1,  0 }, W },
    };

    for (auto [off, dir] : nbs)
    {
        auto [min, max] = make_neighbor_tile_bbox(pos, size, {1,1}, dir);
        if (is_passable(w, ch, min, max, own_id, p))
            ns.neighbors[ns.size++] = coord + off;
    }

    return ns;
}

auto path_search::bbox_union(bbox<float> bb, Vector2i coord, Vector2b offset, Vector2ub size) -> bbox<float>
{
    auto center = coord * iTILE_SIZE2 + Vector2i(offset);
    auto min = center - Vector2i(size / 2u);
    auto max = center + Vector2i(size);
    return {
        .min = Math::min(bb.min, Vector2(min)),
        .max = Math::max(bb.max, Vector2(max)),
    };
}

void path_search::fill_cache_(world& w, chunk_coords_ coord, Vector2ub own_size_, object_id own_id, const pred& p)
{
    auto own_size = Math::max(Vector2i(own_size_), path_search::min_size);

    int32_t x = coord.x, y = coord.y;
    int8_t z = coord.z;

    auto off = Vector2i(x - cache.start.x(), y - cache.start.y());
    fm_debug_assert(off >= Vector2i{} && off < cache.size);
    static_assert(iTILE_SIZE2 / div * div == iTILE_SIZE2);
    auto ch = chunk_coords_{(int16_t)x, (int16_t)y, z};
    auto* c = w.at(ch);
    auto nb = w.neighbors(ch);

    if (!c && std::all_of(nb.begin(), nb.end(), [](const auto& c) { return c.c == nullptr; }))
        return;

    const auto [min_N_, max_N_] = get_value(Vector2i(own_size), Vector2ub(div), rotation::N);
    const auto [min_W_, max_W_] = get_value(Vector2i(own_size), Vector2ub(div), rotation::W);

    const auto min_N = Vector2(min_N_), max_N = Vector2(max_N_),
               min_W = Vector2(min_W_), max_W = Vector2(max_W_);

    enum : unsigned { N, E, S, W };

    constexpr auto tile_start = TILE_SIZE2/-2;
    constexpr auto part_size = Vector2(iTILE_SIZE2/Vector2i(div));

    auto& bits = cache.array[off.y()*cache.size.x()+off.x()];
    constexpr auto stride = TILE_MAX_DIM*(size_t)div;

    for (auto j = 0uz; j < TILE_MAX_DIM; j++)
    {
        for (auto i = 0uz; i < TILE_MAX_DIM; i++)
        {
            const auto pos_ = tile_start + Vector2(i, j) * TILE_SIZE2;

            for (auto ky = 0uz; ky < div; ky++)
            {
                for (auto kx = 0uz; kx < div; kx++)
                {
                    auto pos = pos_ + part_size*Vector2(kx, ky);
                    auto bb_N = bbox<float> { pos + min_N, pos + max_N };
                    auto bb_W = bbox<float> { pos + min_W, pos + max_W };
                    bool b_N = is_passable_(c, nb, bb_N.min, bb_N.max, own_id, p),
                         b_W = is_passable_(c, nb, bb_W.min, bb_W.max, own_id, p);
                    auto jj = j * div + ky, ii = i * div + kx;
                    auto index = jj * stride + ii;
                    fm_debug_assert(index < bits.can_go_north.size());
                    bits.can_go_north[index] = b_N;
                    bits.can_go_west[index] = b_W;
                }
            }
        }
    }
}

void path_search::fill_cache(world& w, Vector2i cmin, Vector2i cmax, int8_t z,
                             Vector2ub own_size, object_id own_id, const pred& p)
{
    for (int32_t y = cmin.y(); y <= cmax.y(); y++)
        for (int32_t x = cmin.x(); x <= cmax.x(); x++)
            fill_cache_(w, {(int16_t)x, (int16_t)y, z}, own_size, own_id, p);
}

Optional<path_search_result> path_search::dijkstra(world& w, Vector2ub own_size, object_id own_id,
                                                   global_coords from, Vector2b from_offset,
                                                   global_coords to, Vector2b to_offset,
                                                   const pred& p)
{
    fm_assert(from.x <= to.x && from.y <= to.y);

    if (from.z() != to.z()) [[unlikely]]
        return {};
    // todo try removing this eventually
    if (from.z() != 0) [[unlikely]]
        return {};

    // check if obj can actually move at all
    if (!is_passable(w, from, from_offset, own_size, own_id, p))
        return {};
    if (!is_passable(w, to, to_offset, own_size, own_id, p))
        return {};

    ensure_allocated(from.chunk(), to.chunk());
    auto [cmin, cmax] = Math::minmax(Vector2i(from.chunk()) - Vector2i(1, 1),
                                     Vector2i(to.chunk()) + Vector2i(1, 1));
    fill_cache(w, cmin, cmax, from.z(), own_size, own_id, p);

    // todo...
    return {};
}

Optional<path_search_result> path_search::dijkstra(world& w, const object& obj,
                                                   global_coords to, Vector2b to_offset,
                                                   const pred& p)
{
    constexpr auto full_tile = Vector2ub(iTILE_SIZE2*3/4);
    auto size = Math::max(obj.bbox_size, full_tile);

    // todo fixme
    // if bbox_offset is added to obj's offset, then all coordinates in the paths are shifted by bbox_offset.
    // maybe add handling to subtract bbox_offset from the returned path.
    // for that it needs to be passed into callees separately.
    fm_assert(obj.bbox_offset.isZero());
    return dijkstra(w, size, obj.id, obj.coord, obj.offset, to, to_offset, p);
}

} // namespace floormat
