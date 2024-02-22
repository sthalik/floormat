#include "../tests-private.hpp"
#include "src/tile-constants.hpp"
#include <bitset>

namespace floormat::tests {

namespace {

constexpr int div_factor = 4; // from path-search.hpp
constexpr auto div_size = iTILE_SIZE2 / div_factor;
constexpr uint32_t chunk_nbits = TILE_MAX_DIM*TILE_MAX_DIM*uint32_t{div_factor*div_factor};

template<typename T> constexpr inline auto tile_size = Math::Vector2<T>{iTILE_SIZE2};
template<typename T> constexpr inline auto chunk_size = Math::Vector2<T>{TILE_MAX_DIM} * tile_size<T>;

constexpr Vector2i chunk_offsets[3][3] = { // from src/raycast.cpp
    {
        { -chunk_size<int>.x(), -chunk_size<int>.y()    },
        { -chunk_size<int>.x(),  0                      },
        { -chunk_size<int>.x(),  chunk_size<int>.y()    },
    },
    {
        { 0,                    -chunk_size<int>.y()    },
        { 0,                     0                      },
        { 0,                     chunk_size<int>.y()    },
    },
    {
        {  chunk_size<int>.x(), -chunk_size<int>.y()    },
        {  chunk_size<int>.x(),  0                      },
        {  chunk_size<int>.x(),  chunk_size<int>.y()    },
    },
};
//static_assert(chunk_offsets[0][0] == Vector2i(-1024, -1024));
//static_assert(chunk_offsets[2][0] == Vector2i(1024, -1024));

constexpr int8_t div_min = -div_factor*2, div_max = div_factor*2; // from src/dijkstra.cpp
//for (int8_t y = div_min; y <= div_max; y++)
//    for (int8_t x = div_min; x <= div_max; x++)

struct pending_s
{
    chunk_coords_ c;
};

struct result_s
{
    chunk_coords_ c;
    std::bitset<chunk_nbits> is_passable;
};

#if 0
struct region_test : base_test
{
    result_s result;
    pending_s pending;

    ~region_test() noexcept override;

    bool handle_key(app&, const key_event&, bool) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void draw_ui(app&, float) override;
    void update_pre(app&) override;
    void update_post(app& a) override;
};
#endif

} // namespace

//Pointer<base_test> tests_data::make_test_region() { return Pointer<region_test>{InPlaceInit}; }

} // namespace floormat::tests
