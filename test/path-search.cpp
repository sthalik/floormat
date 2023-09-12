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
    constexpr auto sample = [](chunk& c, search::bbox bb) {
        return search::sample_rtree_1(c, bb.min, bb.max, (object_id)-1);
    };

    constexpr auto sample2 = [](world& w, chunk_coords_ ch, search::bbox bb) {
        return search::sample_rtree(w, ch, bb.min, bb.max, (object_id)-1);
    };

    constexpr auto bbox = [](Vector2i coord, rotation r) {
        return search::make_neighbor_tile_bbox(coord, {}, r);
    };

    const auto metal2 = loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked);
    const auto table  = loader.scenery("table1");

    {
        using enum rotation;
        auto w = world();
        [[maybe_unused]] auto& c12 = w[chunk_coords_{1, 2, 0}];
        [[maybe_unused]] auto& c11 = w[chunk_coords_{1, 1, 0}];
        c12[{0, 0}].wall_north() = {metal2, 0};

        fm_assert(  sample2(w, chunk_coords_{1, 1, 0}, bbox({0, TILE_MAX_DIM-1}, W)) );
        fm_assert(  sample2(w, chunk_coords_{1, 1, 0}, bbox({0, TILE_MAX_DIM-1}, E)) );
        fm_assert(  sample2(w, chunk_coords_{1, 1, 0}, bbox({0, TILE_MAX_DIM-1}, N)) );
        fm_assert( !sample2(w, chunk_coords_{1, 1, 0}, bbox({0, TILE_MAX_DIM-1}, S)) );

        fm_assert( !sample(c12, bbox({0, 0}, N)) );
        fm_assert(  sample(c12, bbox({0, 0}, E)) );
        fm_assert(  sample(c12, bbox({0, 0}, S)) );
        fm_assert(  sample(c12, bbox({0, 0}, W)) );
    }
    // todo use test chunk
}

} // namespace

void test_app::test_path_search()
{
    test_bbox();
}

} // namespace floormat
