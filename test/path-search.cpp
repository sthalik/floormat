#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/scenery.hpp"
#include "src/path-search.hpp"

namespace floormat {

namespace {

constexpr auto div = path_search::subdivide_factor;
template<typename T> using bbox = path_search::bbox<T>;

void test_bbox()
{
    static constexpr auto is_passable_1 = [](chunk& c, bbox<float> bb) {
        return path_search::is_passable_1(c, bb.min, bb.max, (object_id)-1);
    };

    static constexpr auto is_passable = [](world& w, chunk_coords_ ch, bbox<float> bb) {
        return path_search::is_passable(w, ch, bb.min, bb.max, (object_id)-1);
    };

    static constexpr auto bbox = [](Vector2i coord, rotation r) {
        return path_search::neighbor_tile_bbox(coord, {}, { 1, 1 }, r);
    };

    constexpr auto neighbor_tiles = [](world& w, chunk_coords_ ch, Vector2i pos) {
        return path_search::neighbor_tiles(w, { ch, pos }, {}, (object_id)-1);
    };

    const auto metal2 = loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked);
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
            c12[{0, 0}].wall_north() = {metal2, 0};

            fm_assert( !is_passable_1(c12, bbox({}, N)) );
            fm_assert(  is_passable_1(c12, bbox({}, E)) );
            fm_assert(  is_passable_1(c12, bbox({}, S)) );
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

        c[{8, 7}].wall_north() = {metal2,0};
        c[{8, 9}].wall_north() = {metal2,0};
        fm_assert(  is_passable_1(c, bbox({8, 6}, N)) );
        fm_assert( !is_passable_1(c, bbox({8, 6}, S)) );
        fm_assert( !is_passable_1(c, bbox({8, 7}, N)) );

        fm_assert(  is_passable_1(c, bbox({8, 8}, N)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, E)) );
        fm_assert( !is_passable_1(c, bbox({8, 8}, S)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, W)) );

        fm_assert(neighbor_tiles(w, ch, {8, 8}).size == 3);

        c[{8, 8}].wall_north() = {metal2,0};
        c.mark_passability_modified();
        fm_assert(  is_passable_1(c, bbox({8, 8}, C)) );
        fm_assert( !is_passable_1(c, bbox({8, 7}, S)) );

        fm_assert( !is_passable_1(c, bbox({8, 8}, N)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, E)) );
        fm_assert( !is_passable_1(c, bbox({8, 8}, S)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, W)) );

        fm_assert(neighbor_tiles(w, ch, {8, 8}).size == 2);
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
            fm_assert(is_passable_1(c, bbox(coord, S)) == dirs[2]);
            fm_assert(is_passable_1(c, bbox(coord, W)) == dirs[3]);
        };

        is_passable_NESW(c, {8, 8}, { false, false, false, false });
        is_passable_NESW(c, {8, 9}, { false, true,  true,  true  });
        is_passable_NESW(c, {2, 4}, { true,  false, true,  true  });
        is_passable_NESW(c, {4, 4}, { true,  true,  true,  false });

        fm_assert(neighbor_tiles(w, ch, {8, 8}).size == 0);
        fm_assert(neighbor_tiles(w, ch, {8, 9}).size == 3);
        fm_assert(neighbor_tiles(w, ch, {2, 4}).size == 3);
        fm_assert(neighbor_tiles(w, ch, {4, 4}).size == 3);
    }
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
}

} // namespace

void test_app::test_path_search()
{
    test_bbox();
}

} // namespace floormat
