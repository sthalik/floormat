#include "../tests-private.hpp"
#include "src/tile-constants.hpp"
#include "src/chunk.hpp"
#include "src/path-search.hpp"
#include <bitset>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>

namespace floormat::tests {

namespace {

using detail_astar::div_factor;
using detail_astar::div_size;
template<typename T> using bbox = typename path_search::bbox<T>;

constexpr uint32_t chunk_dim_nbits = TILE_MAX_DIM*uint32_t{div_factor},
                   chunk_nbits = chunk_dim_nbits*chunk_dim_nbits;
constexpr auto bbox_size = Vector2ub(iTILE_SIZE2/2);

//for (int8_t y = div_min; y <= div_max; y++)
//    for (int8_t x = div_min; x <= div_max; x++)

constexpr bbox<Int> bbox_from_pos1(Vector2i center, Vector2ui size) // from src/dijkstra.cpp
{
    auto top_left = center - Vector2i(size / 2);
    auto bottom_right = top_left + Vector2i(size);
    return { top_left, bottom_right };
}

constexpr bbox<Int> bbox_from_pos2(Vector2i pt, Vector2i from, Vector2ui size) // from src/dijkstra.cpp
{
    auto bb0 = bbox_from_pos1(from, size);
    auto bb = bbox_from_pos1(pt, size);
    auto min = Math::min(bb0.min, bb.min);
    auto max = Math::max(bb0.max, bb.max);
    return { min, max };
}

struct pending_s
{
    chunk_coords_ c;
};

struct result_s
{
    chunk_coords_ c;
    std::bitset<chunk_nbits> is_passable;
};

struct region_test : base_test
{
    result_s result;
    pending_s pending;

    ~region_test() noexcept override = default;

    bool handle_key(app&, const key_event&, bool) override { return false; }
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override { return false; }
    bool handle_mouse_move(app& a, const mouse_move_event& e) override { return false; }
    void draw_overlay(app& a) override {}
    void draw_ui(app&, float) override {}
    void update_pre(app&) override {}
    void update_post(app& a) override {}
};

} // namespace

Pointer<base_test> tests_data::make_test_region() { return Pointer<region_test>{InPlaceInit}; }

} // namespace floormat::tests
