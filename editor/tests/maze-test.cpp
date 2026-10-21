#include "../tests-private.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "../mazes.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/search-astar.hpp"
#include "src/search-pred.hpp"
#include "src/search-result.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/point.inl"
#include "src/world.hpp"
#include "floormat/main.hpp"
#include <cr/Optional.h>

namespace floormat::tests {

using namespace floormat::imgui;

namespace {

struct maze_test final : base_test
{
    ~maze_test() noexcept override = default;

    bool handle_key(app&, const key_event&, bool) override { return false; }
    bool handle_mouse_click(app&, const mouse_button_event&, bool) override { return false; }
    bool handle_mouse_move(app&, const mouse_move_event&) override { return false; }
    void draw_overlay(app&) override {}
    void draw_ui(app& a, float menu_bar_height) override;
    void update_pre(app&, const Ns&) override {}
    void update_post(app&, const Ns&) override {}
    Value advance(app& a, Value v) override;

    void build(app& a);
    void solve(app& a);

    uint64_t seed = 1;
    Optional<mazes::route> route;
};

void maze_test::build(app& a)
{
    auto& m = a.main();
    auto& w = m.world();
    auto C = a.ensure_player_character(w);
    auto& pool = m.astar().pool_registry().pool_for(Math::max(C->bbox_size.x(), C->bbox_size.y()));
    route = mazes::replace(w, {0, 0, 0}, mazes::exit_cells, seed, pool, C->id);
    auto i = C->index();
    C->teleport_to(i, route->from, rotation_COUNT);
    a.center_camera_on(route->from);
}

// max_dist only bounds a search from the entrance: from a dead end the way out can be longer.
void maze_test::solve(app& a)
{
    auto& m = a.main();
    auto& w = m.world();
    auto C = a.ensure_player_character(w);
    auto i = C->index();
    C->teleport_to(i, route->from, rotation_COUNT);
    a.center_camera_on(route->from);
    const auto bb = Vector2ui{C->bbox_size};
    auto res = m.astar().Dijkstra(w, route->from, route->to, route->max_dist, bb, Search::without_critters());
    if (!res.is_found())
    {
        fm_warn("maze: seed %llu has no path", (unsigned long long)seed);
        return;
    }
    (void)m.astar().string_pull(w, res, bb, Search::without_critters());
    C->speed = 50;
    C->script.do_reassign(critter_script::make_walk_script(move(res)), C);
}

base_test::Value maze_test::advance(app& a, Value v)
{
    if (v.u32 == 0)
        build(a);
    else
        solve(a);
    return {};
}

void maze_test::draw_ui(app& a, float)
{
    constexpr uint64_t step = 1;
    char buf[32];
    const auto label_width = ImGui::CalcTextSize("MMMM").x;
    ImGui::NewLine();
    label_left("seed", buf, label_width);
    ImGui::InputScalar("##seed", ImGuiDataType_U64, &seed, &step);
    if (ImGui::Button("build"))
        build(a);
    ImGui::SameLine();
    {
        auto d = begin_disabled(!route);
        if (ImGui::Button("solve"))
            solve(a);
    }
    if (route)
        ImGui::Text("route %u tiles", route->bfs_tiles);
}

} // namespace

Pointer<base_test> tests_data::make_test_maze() { return Pointer<maze_test>{InPlaceInit}; }

} // namespace floormat::tests
