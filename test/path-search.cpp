#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
#include "src/world.hpp"
#include "src/scenery.hpp"
#include "src/path-search-bbox.hpp"
#include <Magnum/Math/Functions.h>

namespace floormat {

using namespace floormat::detail_astar;
using detail_astar::bbox;

namespace {

constexpr bbox<int> get_value(Vector2i sz, Vector2ui div, rotation r)
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
    [[maybe_unused]] constexpr auto min_N = N.min + shift, max_N = N.max + shift;
    [[maybe_unused]] constexpr auto N_min_x = min_N.x(), N_min_y = min_N.y();
    [[maybe_unused]] constexpr auto N_max_x = max_N.x(), N_max_y = max_N.y();

    [[maybe_unused]] constexpr auto E = get_value(sz, {1,1}, rotation::E);
    [[maybe_unused]] constexpr auto min_E = E.min + shift, max_E = E.max + shift;
    [[maybe_unused]] constexpr auto E_min_x = min_E.x(), E_min_y = min_E.y();
    [[maybe_unused]] constexpr auto E_max_x = max_E.x(), E_max_y = max_E.y();

    [[maybe_unused]] constexpr auto S = get_value(sz, {1,1}, rotation::S);
    [[maybe_unused]] constexpr auto min_S = S.min + shift, max_S = S.max + shift;
    [[maybe_unused]] constexpr auto S_min_x = min_S.x(), S_min_y = min_S.y();
    [[maybe_unused]] constexpr auto S_max_x = max_S.x(), S_max_y = max_S.y();

    [[maybe_unused]] constexpr auto W = get_value(sz, {1,1}, rotation::W);
    [[maybe_unused]] constexpr auto min_W = W.min + shift, max_W = W.max + shift;
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
    constexpr auto tile_start = iTILE_SIZE2/-2;
    constexpr auto sz = Vector2i(8, 16);

    {
        constexpr auto bb = get_value(sz, Vector2ui(div_factor), N);
        constexpr auto min = tile_start + bb.min, max = tile_start + Vector2i(bb.max);
        static_assert(min.x() == -32 - sz.x()/2);
        static_assert(max.x() == -32 + sz.x()/2);
        static_assert(min.y() == -48 - sz.y()/2);
        static_assert(max.y() == -32 + sz.y()/2);
    }
    {
        constexpr auto bb = get_value(sz, Vector2ui(div_factor), W);
        constexpr auto min = tile_start + bb.min, max = tile_start + bb.max;
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

auto neighbor_tile_bbox(Vector2i coord, Vector2i own_size, Vector2ui div, rotation r) -> bbox<float>
{
    own_size = Math::max(own_size, Vector2i(min_size));
    const auto shift = coord * iTILE_SIZE2;
    auto [min, max] = get_value(own_size, div, r);
    return { Vector2(min + shift), Vector2(max + shift) };
}

auto neighbor_tiles(world& w, global_coords coord, Vector2i size, object_id own_id, const pred& p) -> neighbors
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
        auto [min, max] = neighbor_tile_bbox(pos, size, { 1, 1 }, r);
        if (path_search::is_passable(w, ch, {min, max}, own_id, p))
            ns.data[ns.size++] = { coord + off, {} };
    }

    return ns;
}

void test_bbox()
{
    static constexpr auto is_passable_1 = [](chunk& c, bbox<float> bb) {
        return path_search::is_passable_1(c, bb.min, bb.max, (object_id)-1);
    };

    static constexpr auto is_passable = [](world& w, chunk_coords_ ch, bbox<float> bb) {
        return path_search::is_passable(w, ch, bb, (object_id)-1);
    };

    static constexpr auto bbox = [](Vector2i coord, rotation r) {
        return neighbor_tile_bbox(coord, {}, { 1, 1 }, r);
    };

    constexpr auto neighbors = [](world& w, chunk_coords_ ch, Vector2i pos) {
        return neighbor_tiles(w, { ch, pos }, {}, (object_id)-1, never_continue());
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
        auto& c = test_app::make_test_chunk(w, ch);

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

        path_search search;
        search.ensure_allocated({}, {});
        search.fill_cache_(w, {0, 0, 0}, {}, {});

        constexpr auto check_N = [&](path_search& search, chunk_coords ch, local_coords tile, Vector2i subdiv) {
            auto c = search.cache_chunk_index(ch);
            auto t = search.cache_tile_index(tile, subdiv);
            return search.cache.array[c].can_go_north[t];
        };
        constexpr auto check_W = [&](path_search& search, chunk_coords ch, local_coords tile, Vector2i subdiv) {
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

} // namespace

void test_app::test_astar()
{
    test_bbox();
}

} // namespace floormat
