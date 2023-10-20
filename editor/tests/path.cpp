#include "../tests-private.hpp"
#include "../app.hpp"
#include "floormat/main.hpp"
#include "src/path-search.hpp"
#include "src/critter.hpp"

namespace floormat::tests {

bool path_test::handle_key(app& a, const key_event& e)
{
    return false;
}

bool path_test::handle_mouse_click(app& a, const mouse_button_event& e)
{
    switch (e.button)
    {
    case mouse_button_left: {
        auto& M = a.main();
        auto& w = M.world();
        auto& astar = M.astar();
        auto C = a.ensure_player_character(w);
        if (auto pt = a.cursor_state().point())
        {
            constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
            auto pt0 = C->position();
            auto vec = (pt->coord() - pt0.coord()) * iTILE_SIZE2 * 2 + chunk_size * 3;
            auto dist = (uint32_t)vec.length();
            auto res = astar.Dijkstra(w, *pt, pt0, C->id, dist, C->bbox_size, 1);
            if (res)
            {
                active = true;
                from = pt0;
                path = res.path();
            }
            else
            {
                active = false;
                from = {};
                path = {};
            }
        }
        return true;
    }
    case mouse_button_right:
        if (active)
        {
            *this = {};
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool path_test::handle_mouse_move(app& a, const mouse_move_event& e)
{
    return false;
}

void path_test::draw_overlay(app& a)
{

}

void path_test::update_pre(app& a)
{

}

void path_test::update_post(app& a)
{

}

} // namespace floormat::tests
