#include "app.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/scenery.hpp"
#include "src/path-search.hpp"

namespace floormat {

namespace {

void test_bbox()
{
    static constexpr auto is_passable_1 = [](chunk& c, path_search::bbox bb) {
        return path_search::is_passable_1(c, bb.min, bb.max, (object_id)-1);
    };

    static constexpr auto is_passable = [](world& w, chunk_coords_ ch, path_search::bbox bb) {
        return path_search::is_passable(w, ch, bb.min, bb.max, (object_id)-1);
    };

    static constexpr auto bbox = [](Vector2i coord, rotation r) {
        return path_search::make_neighbor_tile_bbox(coord, {}, r);
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

        c[{8, 8}].wall_north() = {metal2,0};
        c.mark_passability_modified();
        fm_assert(  is_passable_1(c, bbox({8, 8}, C)));
        fm_assert( !is_passable_1(c, bbox({8, 7}, S)) );
        fm_assert( !is_passable_1(c, bbox({8, 8}, N)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, E)) );
        fm_assert( !is_passable_1(c, bbox({8, 8}, S)) );
        fm_assert(  is_passable_1(c, bbox({8, 8}, W)) );
    }
    {
        using enum rotation;
        constexpr auto ch = chunk_coords_{0, 0, 0};
        auto w = world();
        auto& c = test_app::make_test_chunk(w, ch);

        constexpr auto is_passable_NESW = [](chunk& c, Vector2i coord, std::array<bool, 4> dirs) {
            fm_assert(is_passable_1(c, bbox(coord, N)) == dirs[0]);
            fm_assert(is_passable_1(c, bbox(coord, E)) == dirs[1]);
            fm_assert(is_passable_1(c, bbox(coord, S)) == dirs[2]);
            fm_assert(is_passable_1(c, bbox(coord, W)) == dirs[3]);
        };

        is_passable_NESW(c, {8, 8}, { false, false, false, false });
        is_passable_NESW(c, {8, 9}, { false, true,  true,  true  });
        is_passable_NESW(c, {2, 4}, { true,  false, true,  true  });
        is_passable_NESW(c, {4, 4}, { true,  true,  true,  false });
    }
}

} // namespace

void test_app::test_path_search()
{
    test_bbox();
}

} // namespace floormat
