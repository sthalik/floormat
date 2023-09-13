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
    constexpr auto is_passable_1 = [](chunk& c, path_search::bbox bb) {
        return path_search::is_passable_1(c, bb.min, bb.max, (object_id)-1);
    };

    constexpr auto is_passable = [](world& w, chunk_coords_ ch, path_search::bbox bb) {
        return path_search::is_passable(w, ch, bb.min, bb.max, (object_id)-1);
    };

    constexpr auto bbox = [](Vector2i coord, rotation r) {
        return path_search::make_neighbor_tile_bbox(coord, {}, r);
    };

    const auto metal2 = loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked);
    const auto table  = loader.scenery("table1");

    {
        constexpr auto coord1 = chunk_coords_{1, 1, 0},
                       coord2 = chunk_coords_{1, 2, 0};
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
    // todo use test chunk
}

} // namespace

void test_app::test_path_search()
{
    test_bbox();
}

} // namespace floormat
