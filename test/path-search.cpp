#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
#include "src/world.hpp"
#include "src/scenery-proto.hpp"
#include "src/search-constants.hpp"
#include "src/search.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/sweep-aabb.hpp"
#include "src/raycast-line.hpp"
#include "src/chunk.hpp"
#include "src/point.inl"
#include "loader/scenery-cell.hpp"
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

using namespace floormat::Search;

namespace {

constexpr Range2Di get_value(Vector2i sz, Vector2ui div, rotation r)
{
    const int offset_W = iTILE_SIZE2.x()/(int)div.x(), offset_N = iTILE_SIZE2.y()/(int)div.y();

    const auto rʹ = (uint8_t)r;
    [[assume(rʹ <= (uint8_t)rotation_COUNT)]];

    switch (rʹ)
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
        auto min_C = Vector2i(-sz.x()/2,                        -sz.y()/2                       );
        auto max_C = min_C + Vector2i(sz);
        return {min_C, max_C};
    }
    default:
        fm_abort("wrong 4-way rotation enum '%d'", (int)r);
    }
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4189)
#endif

constexpr bool test_offsets()
{
    constexpr auto sz = Vector2i(min_size);
    constexpr Vector2i shift = Vector2i(0, 0) * iTILE_SIZE2 + Vector2i(0, 0);

    [[maybe_unused]] constexpr auto N = get_value(sz, {1,1}, rotation::N);
    [[maybe_unused]] constexpr auto min_N = N.min() + shift, max_N = N.max() + shift;
    [[maybe_unused]] constexpr auto N_min_x = min_N.x(), N_min_y = min_N.y();
    [[maybe_unused]] constexpr auto N_max_x = max_N.x(), N_max_y = max_N.y();

    [[maybe_unused]] constexpr auto E = get_value(sz, {1,1}, rotation::E);
    [[maybe_unused]] constexpr auto min_E = E.min() + shift, max_E = E.max() + shift;
    [[maybe_unused]] constexpr auto E_min_x = min_E.x(), E_min_y = min_E.y();
    [[maybe_unused]] constexpr auto E_max_x = max_E.x(), E_max_y = max_E.y();

    [[maybe_unused]] constexpr auto S = get_value(sz, {1,1}, rotation::S);
    [[maybe_unused]] constexpr auto min_S = S.min() + shift, max_S = S.max() + shift;
    [[maybe_unused]] constexpr auto S_min_x = min_S.x(), S_min_y = min_S.y();
    [[maybe_unused]] constexpr auto S_max_x = max_S.x(), S_max_y = max_S.y();

    [[maybe_unused]] constexpr auto W = get_value(sz, {1,1}, rotation::W);
    [[maybe_unused]] constexpr auto min_W = W.min() + shift, max_W = W.max() + shift;
    [[maybe_unused]] constexpr auto W_min_x = min_W.x(), W_min_y = min_W.y();
    [[maybe_unused]] constexpr auto W_max_x = max_W.x(), W_max_y = max_W.y();

    return true;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

constexpr bool test_offsets2()
{
    using enum rotation;
    constexpr auto tile_start = -half_tile<Vector2i>;
    constexpr auto sz = Vector2i(8, 16);

    {
        constexpr auto bb = get_value(sz, Vector2ui(div_factor), N);
        constexpr auto min = tile_start + bb.min(), max = tile_start + Vector2i(bb.max());
        static_assert(min.x() == -32 - sz.x()/2);
        static_assert(max.x() == -32 + sz.x()/2);
        static_assert(min.y() == -48 - sz.y()/2);
        static_assert(max.y() == -32 + sz.y()/2);
    }
    {
        constexpr auto bb = get_value(sz, Vector2ui(div_factor), W);
        constexpr auto min = tile_start + bb.min(), max = tile_start + bb.max();
        static_assert(min.x() == -32 - 16 - sz.x()/2);
        static_assert(max.x() == -32 + sz.x()/2);
        static_assert(min.y() == -32 - sz.y()/2);
        static_assert(max.y() == -32 + sz.y()/2);
    }

    return true;
}

struct neighbors final
{
    std::array<global_coords, 5> data;
    uint8_t size = 0;
};

auto neighbor_tile_bbox(Vector2i coord, Vector2i own_size, Vector2ui div, rotation r) -> Range2D
{
    own_size = Math::max(own_size, Vector2i(min_size));
    const auto shift = coord * iTILE_SIZE2;
    auto range = get_value(own_size, div, r);
    auto min = range.min(), max = range.max();
    return { Vector2(min + shift), Vector2(max + shift) };
}

auto neighbor_tiles(world& w, global_coords coord, Vector2i size, const pred& p) -> neighbors
{
    auto ch = chunk_coords_{ coord.chunk(), coord.z() };
    auto pos = Vector2i(coord.local());
    size = Math::max(size, div_size);

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

    for (auto [off, r] : nbs)
    {
        auto range = neighbor_tile_bbox(pos, size, { 1, 1 }, r);
        if (Search::is_passable(w, ch, range, p))
            ns.data[ns.size++] = { coord + off, {} };
    }

    return ns;
}

void test_bbox()
{
    static constexpr auto is_passable_1 = [](chunk& c, Range2D bb) {
        return Search::is_passable_1(c, bb.min(), bb.max());
    };

    static constexpr auto is_passable = [](world& w, chunk_coords_ ch, Range2D bb) {
        return Search::is_passable(w, ch, bb);
    };

    static constexpr auto bbox = [](Vector2i coord, rotation r) {
        return neighbor_tile_bbox(coord, {}, { 1, 1 }, r);
    };

    constexpr auto neighbors = [](world& w, chunk_coords_ ch, Vector2i pos) {
        return neighbor_tiles(w, { ch, pos }, {}, never_continue());
    };

    const auto wall = loader.invalid_wall_atlas().atlas;
    const auto table  = loader.scenery("table1");

    {
        constexpr auto coord1 = chunk_coords_{10, 11, 0},
                       coord2 = chunk_coords_{10, 12, 0};
        constexpr auto _15 = TILE_MAX_DIM-1;
        using enum rotation;
        {
            auto w = world();
            [[maybe_unused]] auto& c12 = w[coord2];
            [[maybe_unused]] auto& c11 = w[coord1];
            c12[{0, 0}].wall_north() = { wall, 0};

            fm_assert( !is_passable_1(c12, bbox({}, N)) );
            fm_assert(  is_passable_1(c12, bbox({}, E)) );
            //fm_assert(  is_passable_1(c12, bbox({}, S)) );
            fm_assert(  is_passable_1(c12, bbox({}, W)) );

            fm_assert(  is_passable(w, coord1, bbox({0, _15}, N)) );
            fm_assert(  is_passable(w, coord1, bbox({0, _15}, E)) );
            fm_assert( !is_passable(w, coord1, bbox({0, _15}, S)) );
            fm_assert(  is_passable(w, coord1, bbox({0, _15}, W)) );
        }
    }
    {
        using enum rotation;
        constexpr auto C = rotation_COUNT;
        constexpr auto ch = chunk_coords_{21, 22, 0};

        auto w = world();
        auto& c = w[ch];

        c[{8, 7}].wall_north() = { wall,0};
        c[{8, 9}].wall_north() = { wall,0};
        fm_assert(  is_passable_1(c, bbox({8, 6}, N)) );
        fm_assert( !is_passable_1(c, bbox({8, 6}, S)) );
        fm_assert( !is_passable_1(c, bbox({8, 7}, N)) );

        fm_assert(  is_passable_1(c, bbox({8, 8}, N)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, E)) );
        fm_assert( !is_passable_1(c, bbox({8, 8}, S)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, W)) );

        fm_assert(neighbors(w, ch, {8, 8}).size == 3);

        c[{8, 8}].wall_north() = { wall,0};
        c.mark_passability_modified();
        fm_assert(  is_passable_1(c, bbox({8, 8}, C)) );
        fm_assert( !is_passable_1(c, bbox({8, 7}, S)) );

        fm_assert( !is_passable_1(c, bbox({8, 8}, N)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, E)) );
        fm_assert( !is_passable_1(c, bbox({8, 8}, S)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, W)) );

        fm_assert(neighbors(w, ch, {8, 8}).size == 2);
    }
    {
        using enum rotation;
        constexpr auto ch = chunk_coords_{0, 0, 0};
        auto w = world();
        auto& c = Test::make_test_chunk(w, ch);

        constexpr auto is_passable_NESW = [](chunk& c, Vector2i coord, std::array<bool, 4> dirs)
        {
            fm_assert(is_passable_1(c, bbox(coord, N)) == dirs[0]);
            fm_assert(is_passable_1(c, bbox(coord, E)) == dirs[1]);
            //fm_assert(is_passable_1(c, bbox(coord, S)) == dirs[2]);
            fm_assert(is_passable_1(c, bbox(coord, W)) == dirs[3]);
        };

        is_passable_NESW(c, {8, 8}, { false, false, false, false });
        is_passable_NESW(c, {8, 9}, { false, true,  true,  true  });
        is_passable_NESW(c, {2, 4}, { true,  false, true,  true  });
        is_passable_NESW(c, {4, 4}, { true,  true,  true,  false });

        fm_assert(neighbors(w, ch, {8, 8}).size == 0);
        //fm_assert(neighbors(w, ch, {8, 9}).size == 3);
        fm_assert(neighbors(w, ch, {8, 9}).size >= 2);
        fm_assert(neighbors(w, ch, {2, 4}).size == 3);
        fm_assert(neighbors(w, ch, {4, 4}).size == 3);
    }

    fm_assert(test_offsets2());
    fm_assert(test_offsets());

#if 0
    {
        constexpr auto ch = chunk_coords_{};
        auto w = world();
        auto& c = w[ch];
        constexpr size_t K = 8;
        const auto wall = tile_image_proto{metal2, 0};
        c[{0,   0  }].wall_north() = wall;
        c[{0,   0  }].wall_west() = wall;
        c[{K,   K  }].wall_north() = { metal2, 0 };
        c[{K,   K  }].wall_west()  = { metal2, 0 };
        c[{K,   K+1}].wall_north() = { metal2, 0 };
        c[{K+1, K  }].wall_west()  = { metal2, 0 };

        Search search;
        search.ensure_allocated({}, {});
        search.fill_cache_(w, {0, 0, 0}, {}, {});

        constexpr auto check_N = [&](Search& search, chunk_coords ch, local_coords tile, Vector2i subdiv) {
            auto c = search.cache_chunk_index(ch);
            auto t = search.cache_tile_index(tile, subdiv);
            return search.cache.array[c].can_go_north[t];
        };
        constexpr auto check_W = [&](Search& search, chunk_coords ch, local_coords tile, Vector2i subdiv) {
            auto c = search.cache_chunk_index(ch);
            auto t = search.cache_tile_index(tile, subdiv);
            return search.cache.array[c].can_go_west[t];
        };

        static_assert(div == 4);
        constexpr Vector2i s00 = {0,0}, s01 = {0,2}, s10 = {2,0}, s11 = {2,2}, s22 = {3,3};

        fm_assert( !check_N(search, {}, {  0,   0}, s10 ));
        fm_assert( !check_W(search, {}, {  0,   0}, s01 ));
        fm_assert(  check_N(search, {}, {  0,   1}, s10 ));
        fm_assert(  check_W(search, {}, {  1,   0}, s01 ));

        fm_assert( !check_N(search, {}, {  0,   1}, s00 ));
        fm_assert( !check_W(search, {}, {  1,   0}, s00 ));

        fm_assert(  check_W(search, {}, {K-1, K  }, s01 ));
        fm_assert(  check_N(search, {}, {K-1, K  }, s10 ));
        fm_assert(  check_W(search, {}, {K,   K-1}, s01 ));
        fm_assert(  check_N(search, {}, {K,   K-1}, s10 ));

        fm_assert(  check_W(search, {}, {K-1, K  }, s22 ));
        fm_assert(  check_N(search, {}, {K-1, K  }, s22 ));
        fm_assert(  check_W(search, {}, {K,   K-1}, s22 ));
        fm_assert(  check_N(search, {}, {K,   K-1}, s22 ));

        fm_assert( !check_N(search, {}, {K,   K  }, s10 ));
        fm_assert( !check_W(search, {}, {K,   K  }, s01 ));
        fm_assert(  check_N(search, {}, {K,   K  }, s11 ));
        fm_assert(  check_W(search, {}, {K,   K  }, s11 ));

        fm_assert( !check_W(search, {}, {K+1, K  }, s01 ));
        fm_assert(  check_N(search, {}, {K+1, K  }, s10 ));
        fm_assert( !check_N(search, {}, {K,   K+1}, s10 ));
        fm_assert(  check_W(search, {}, {K,   K+1}, s01 ));
        fm_assert(  check_N(search, {}, {K+2, K  }, s10 ));
        fm_assert(  check_W(search, {}, {K+2, K  }, s01 ));
        fm_assert(  check_N(search, {}, {K,   K+2}, s10 ));
        fm_assert(  check_W(search, {}, {K,   K+2}, s01 ));

        fm_assert(  check_N(search, {}, {K+1, K+2  }, s00 ));
        fm_assert(  check_N(search, {}, {K+1, K+2  }, s10 ));
        fm_assert(  check_W(search, {}, {K+1, K+2  }, s00 ));

        fm_assert(  check_N(search, {}, {K+2, K+2  }, s00 ));
        fm_assert(  check_W(search, {}, {K+2, K+2  }, s00 ));
    }
#endif
}

constexpr auto pull_ch = chunk_coords_{0, 0, 0};
constexpr auto pull_bbox = Vector2ui{32};
constexpr uint32_t pull_max_dist = 4096;

world make_pull_world()
{
    auto w = world();
    for (int16_t j = -1; j <= 1; j++)
        for (int16_t i = -1; i <= 1; i++)
            (void)w[chunk_coords_{i, j, 0}];
    return w;
}

void add_pillar(world& w, local_coords tile, Vector2ub size, Vector2b offset = {})
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_size   = size;
    p.bbox_offset = offset;
    p.pass        = pass_mode::blocked;
    w.make_scenery(w.make_id(), {pull_ch, tile}, move(p));
}

bool is_segment_clear(world& w, point a, point b, Vector2ui size)
{
    const auto ch = a.chunk3();
    const auto origin = point{ch, {}, {}};
    const auto line = rc::bresenham_line{a - origin, b - origin};
    return !find_swept_collider(w, ch, line, 0, line.steps(), size, never_continue()).has_collider;
}

// Every segment is swept without a pool, so against the R-tree everywhere.
void check_pulled(world& w, const path_search_result& res, Vector2ui size)
{
    const auto raw = res.path();
    const auto sp = res.smoothings();
    fm_assert(sp.size() >= 2);
    fm_assert(sp.front().pt == raw.front() && sp.front().raw_index == 0);
    fm_assert(sp.back().pt == raw.back() && sp.back().raw_index == raw.size() - 1);
    float raw_len = 0, sp_len = 0;
    for (auto i = 0uz; i + 1 < raw.size(); i++)
        raw_len += Vector2(raw[i+1] - raw[i]).length();
    for (auto i = 0uz; i + 1 < sp.size(); i++)
    {
        fm_assert(sp[i].raw_index <= sp[i+1].raw_index);
        fm_assert(is_segment_clear(w, sp[i].pt, sp[i+1].pt, size));
        sp_len += Vector2(sp[i+1].pt - sp[i].pt).length();
    }
    fm_assert(sp_len <= raw_len + 1e-3f);
}

path_search_result pull(astar& A, world& w, point from, point to)
{
    auto res = A.Dijkstra(w, from, to, pull_max_dist, pull_bbox, without_critters());
    fm_assert(res.is_found());
    fm_assert(A.string_pull(w, res, pull_bbox, without_critters()));
    check_pulled(w, res, pull_bbox);
    return res;
}

void test_string_pull_open()
{
    auto A = astar{};
    auto w = make_pull_world();
    const auto res = pull(A, w, point{pull_ch, {2, 2}, {3, -5}}, point{pull_ch, {9, 5}, {7, 2}});
    fm_assert(res.smoothings().size() == 2);
}

void test_string_pull_pillar()
{
    auto A = astar{};
    auto w = make_pull_world();
    constexpr auto tile = local_coords{7, 6};
    constexpr auto size = Vector2ub{40, 40};
    add_pillar(w, tile, size);
    w[pull_ch].mark_modified();

    const auto res = pull(A, w, point{pull_ch, {2, 6}, {}}, point{pull_ch, {12, 6}, {}});
    const auto sp = res.smoothings();
    fm_assert(sp.size() == 4);
    const auto origin = point{pull_ch, {}, {}};
    const auto center = Vector2i(tile) * iTILE_SIZE2;
    const auto lo = center - Vector2i(Vector2ui(size)/2u + pull_bbox/2u), hi = center + Vector2i(Vector2ui(size)/2u + pull_bbox/2u);
    const auto c1 = sp[1].pt - origin, c2 = sp[2].pt - origin;
    fm_assert(c1.y() == c2.y() && (c1.y() == lo.y() || c1.y() == hi.y()));
    fm_assert(c1.x() == lo.x() && c2.x() == hi.x());
}

// Walls are one rect per tile. Starting 2 px off the face, the first line that misses the end
// corner crosses all of them, and a corner from the nearest one would be a seam.
void test_string_pull_wall()
{
    auto A = astar{};
    auto w = make_pull_world();
    const auto wall = wall_image_proto{loader.invalid_wall_atlas().atlas, 0};
    for (uint8_t x = 4; x <= 9; x++)
        w[pull_ch][{x, 6}].wall_north() = wall;
    w[pull_ch].mark_modified();

    const auto origin = point{pull_ch, {}, {}};
    const auto end = Vector2i{9, 6} * iTILE_SIZE2 + Vector2i{half_tile<int>, -half_tile<int>} + Vector2i(pull_bbox/2u);
    const auto res = pull(A, w, origin + Vector2i{230, end.y() + 2}, origin + Vector2i{660, 300});
    const auto sp = res.smoothings();
    fm_assert(sp.size() == 3);
    fm_assert(sp[1].pt - origin == end);
}

// A raw segment that isn't a run of steps is one step, so the triangle Han takes as clear can
// hold a small rect. It doesn't block the line to the goal, only the one to r1's corner.
void test_string_pull_hidden_corner()
{
    auto w = make_pull_world();
    constexpr auto size = Vector2ui{16};
    constexpr auto r1 = Vector2i{400, 400}, r2 = Vector2i{320, 462};
    constexpr auto r1_size = Vector2ub{40, 40}, r2_size = Vector2ub{8, 8};
    add_pillar(w, {6, 6}, r1_size, Vector2b(r1 - Vector2i{6, 6} * iTILE_SIZE2));
    add_pillar(w, {5, 7}, r2_size, Vector2b(r2 - Vector2i{5, 7} * iTILE_SIZE2));
    w[pull_ch].mark_modified();

    const auto origin = point{pull_ch, {}, {}};
    path_search_result res;
    for (auto px : { Vector2i{200, 500}, Vector2i{600, 500}, Vector2i{600, 300} })
        arrayAppend(res.raw_path(), origin + px);
    auto A = astar{};
    fm_assert(A.string_pull(w, res, size, without_critters()));
    check_pulled(w, res, size);

    const auto& sp = res.smoothings();
    fm_assert(sp.size() == 4);
    fm_assert(sp[1].pt - origin == r2 + Vector2i(Vector2ui(r2_size)/2u + size/2u));
    fm_assert(sp[2].pt - origin == r1 + Vector2i(Vector2ui(r1_size)/2u + size/2u));
}

// The path dips past the pillar's corner, so the line to the next step clips it and the corner is
// added. The goal's line passes 3 px above it, so the turn there flips and the corner goes again.
void test_string_pull_truncate()
{
    auto w = make_pull_world();
    constexpr auto size = Vector2ui{16};
    add_pillar(w, {7, 9}, {84, 84}, {14, -14});
    w[pull_ch].mark_modified();

    const auto origin = point{pull_ch, {}, {}};
    path_search_result res;
    for (auto px : { Vector2i{416, 492}, Vector2i{512, 492}, Vector2i{544, 524}, Vector2i{812, 562} })
        arrayAppend(res.raw_path(), origin + px);
    auto A = astar{};
    fm_assert(A.string_pull(w, res, size, without_critters()));
    check_pulled(w, res, size);
    fm_assert(res.smoothings().size() == 2);
}

void test_string_pull_random()
{
    uint64_t state = 0x9e3779b97f4a7c15;
    const auto rnd = [&](uint32_t n) {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        return (uint32_t)(state % n);
    };
    auto A = astar{};
    uint32_t found = 0;
    for (auto k = 0u; k < 32; k++)
    {
        // odd, so corners are rounded outward
        const auto size = Vector2ui{k % 2 ? 27u : pull_bbox.x()};
        auto w = make_pull_world();
        for (auto i = 0u; i < 14; i++)
            add_pillar(w, {(uint8_t)(2 + rnd(12)), (uint8_t)rnd(16)},
                       {(uint8_t)(2 + 2*rnd(40)), (uint8_t)(2 + 2*rnd(40))},
                       {(int8_t)((int)rnd(41) - 20), (int8_t)((int)rnd(41) - 20)});
        w[pull_ch].mark_modified();
        const auto from = point{pull_ch, {1, (uint8_t)rnd(16)}, {(int8_t)((int)rnd(41) - 20), (int8_t)((int)rnd(41) - 20)}};
        const auto to = point{pull_ch, {14, (uint8_t)rnd(16)}, {(int8_t)((int)rnd(41) - 20), (int8_t)((int)rnd(41) - 20)}};
        auto res = A.Dijkstra(w, from, to, pull_max_dist, size, without_critters());
        if (!res.is_found())
            continue;
        found++;
        fm_assert(A.string_pull(w, res, size, without_critters()));
        check_pulled(w, res, size);
    }
    fm_assert(found >= 16);
}

} // namespace

void Test::test_astar()
{
    test_bbox();
    test_string_pull_open();
    test_string_pull_pillar();
    test_string_pull_wall();
    test_string_pull_hidden_corner();
    test_string_pull_truncate();
    test_string_pull_random();
}

} // namespace floormat
