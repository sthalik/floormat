#include "../tests-private.hpp"
#include "src/tile-constants.hpp"
#include "src/chunk.hpp"
#include "src/path-search.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "floormat/main.hpp"
#include <bitset>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>

namespace floormat::tests {

namespace {

using namespace floormat::imgui;
using detail_astar::div_factor;
using detail_astar::div_size;
template<typename T> using bbox = typename path_search::bbox<T>;

constexpr auto div_min = -iTILE_SIZE2/2, div_max = TILE_MAX_DIM*iTILE_SIZE2 - iTILE_SIZE2/2;

constexpr uint32_t chunk_dim_nbits = TILE_MAX_DIM*uint32_t{div_factor}+1,
                   chunk_nbits = chunk_dim_nbits*chunk_dim_nbits;
constexpr auto bbox_size = Vector2ub(iTILE_SIZE2/2);

constexpr auto chunk_size = iTILE_SIZE2*Vector2i(TILE_MAX_DIM),
               div_count = chunk_size * div_factor;

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
    bool exists : 1 = false;
};

struct result_s
{
    std::bitset<chunk_nbits> is_passable;
    chunk_coords_ c;
    bool exists : 1 = false;
};

struct node_s
{
    Vector2s bbox, from;
};

struct tmp_s
{
    Array<node_s> stack;
    std::bitset<chunk_nbits> exists;
    std::array<chunk*, 8> neighbors = {};

    void append(bbox<Int> bbox);
    static Pointer<tmp_s> make();
    void clear();
};

void tmp_s::append(bbox<Int> bb)
{
    arrayAppend(stack, {Vector2s(bb.min), Vector2s(bb.max)});
}

Pointer<tmp_s> tmp_s::make()
{
    auto ptr = Pointer<tmp_s>{InPlace};
    arrayResize(ptr->stack, 0);
    arrayReserve(ptr->stack, TILE_COUNT);
    ptr->exists = {};
    ptr->neighbors = {};
    return ptr;
}

void tmp_s::clear()
{
    arrayResize(stack, 0);
    exists = {};
    neighbors = {};
}

struct region_test : base_test
{
    result_s result;
    pending_s pending;
    Pointer<tmp_s> tmp;

    tmp_s& get_tmp();
    static bool is_passable(chunk* c, const std::array<chunk*, 8>& neighbors, Vector2i pos);
    void do_region_extraction(world& w, chunk_coords_ coord);

    ~region_test() noexcept override = default;

    bool handle_key(app&, const key_event&, bool) override { return {}; }
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override { return {}; }
    void draw_overlay(app& a) override;
    void draw_ui(app&, float) override;
    void update_pre(app&) override {}
    void update_post(app& a) override;
};

tmp_s& region_test::get_tmp()
{
    if (tmp)
        tmp->clear();
    else
        tmp = tmp_s::make();
    return *tmp;
}

bool region_test::is_passable(chunk* c, const std::array<chunk*, 8>& neighbors, Magnum::Vector2i pos)
{
    auto bb = bbox_from_pos1(pos, Vector2ui(bbox_size));
    return path_search::is_passable_(c, neighbors, Vector2(bb.min), Vector2(bb.max), 0);
}

void region_test::do_region_extraction(world& w, chunk_coords_ coord)
{
    if (auto* c = w.at(coord))
    {
        auto& tmp = get_tmp();
        const auto nbs = w.neighbors(coord);

        constexpr auto make_pos = [](Vector2i ij, Vector2i from) {
            auto pos = div_min + div_size * ij;
            auto pos0 = pos + from*div_size;
            return bbox_from_pos2(pos, pos0, Vector2ui(div_size));
        };

        constexpr auto last = Vector2i(TILE_MAX_DIM-1);
        static_assert(div_count.x() == div_count.y());

        for (Int i = 0; i <= div_count.x(); i++)
        {
            auto pos_x1 = make_pos({i, last.y()}, {0, 1});  // bottom
            auto pos_x0 = make_pos({i, 0}, {0, -1});        // top
            auto pos_1y = make_pos({last.x(), i}, {1, 0});  // right
            auto pos_0y = make_pos({0, i}, {-1, 0});        // left
            tmp.append(pos_x1);
            tmp.append(pos_x0);
            tmp.append(pos_1y);
            tmp.append(pos_0y);
        }
    }
    else
    {
        result.exists = false;
        pending.exists = false;
    }
}

void region_test::draw_overlay(app& a)
{

}

void region_test::draw_ui(app& a, float width)
{

}

bool region_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    if (e.button == mouse_button_left && is_down)
    {
        auto& M = a.main();
        auto& w = M.world();
        if (auto pt_ = a.cursor_state().point())
        {
            pending = {
                .c = pt_->chunk3(),
                .exists = true,
            };
            return true;
        }
    }
    else if (e.button == mouse_button_right && is_down)
    {
        pending.exists = false;
        result.exists = false;
    }
    return false;
}

void region_test::update_post(app& a)
{

}

} // namespace

Pointer<base_test> tests_data::make_test_region() { return Pointer<region_test>{InPlaceInit}; }

} // namespace floormat::tests
