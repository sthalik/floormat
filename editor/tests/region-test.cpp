#include "../tests-private.hpp"
#include "src/tile-constants.hpp"
#include "src/chunk.hpp"
#include "src/path-search-bbox.hpp"
#include "src/object.hpp"
#include "src/world.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "floormat/main.hpp"
#include <bitset>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Color.h>

namespace floormat::tests {

namespace {

using namespace floormat::imgui;
using detail_astar::div_factor;
using detail_astar::div_size;
using detail_astar::bbox;
static_assert((iTILE_SIZE2 % div_size).isZero());

constexpr auto div_count = iTILE_SIZE2 * TILE_MAX_DIM / div_size;
constexpr auto chunk_bits = div_count.product(),
               visited_bits = div_count.product()*4*4;
constexpr auto div_min = -iTILE_SIZE2/2 + div_size/2,
               div_max = iTILE_SIZE2 * TILE_MAX_DIM - iTILE_SIZE2/2 - div_size + div_size/2;
static_assert(div_count.x() == div_count.y());

constexpr bbox<Int> bbox_from_pos1(Vector2i center)
{
    constexpr auto half = div_size/2;
    auto start = center - half;
    return { start, start + div_size };
}

constexpr bbox<Int> bbox_from_pos2(Vector2i pt, Vector2i from/*, Vector2ui size*/) // from src/dijkstra.cpp
{
    auto bb0 = bbox_from_pos1(from/*, size*/);
    auto bb = bbox_from_pos1(pt/*, size*/);
    auto min = Math::min(bb0.min, bb.min);
    auto max = Math::max(bb0.max, bb.max);
    return { min, max };
}

constexpr bbox<Int> make_pos(Vector2i ij, Vector2i from)
{
    auto pos = div_min + div_size * ij;
    auto pos0 = pos + from*div_size;
    return bbox_from_pos2(pos, pos0/*, Vector2ui(div_size)*/);
}

bool check_pos(chunk& c, const std::array<chunk*, 8>& nbs, Vector2i ij, Vector2i from)
{
    auto pos = make_pos(ij, from);
    bool ret = path_search::is_passable_(&c, nbs, Vector2(pos.min), Vector2(pos.max), 0);
    //if (ret) Debug{} << "check" << ij << ij/div_factor << ij % div_factor << pos.min << pos.max << ret;
    //Debug{} << "check" << ij << pos.min << pos.max << ret;
    return ret;
}

struct pending_s
{
    chunk_coords_ c;
    bool exists : 1 = false;
};

struct result_s
{
    std::bitset<chunk_bits> is_passable;
    chunk_coords_ c;
    bool exists : 1 = false;
};

struct node_s
{
    Vector2i pos;
};

struct tmp_s
{
    Array<node_s> stack;
    std::bitset<visited_bits> visited;
    std::bitset<chunk_bits> passable;

    void append(Vector2i pos, int from);
    static Pointer<tmp_s> make();
    void clear();
};

void tmp_s::append(Vector2i pos, int from)
{
    auto i = (uint32_t)pos.y() * (uint32_t)div_count.x() + (uint32_t)pos.x();
    fm_debug_assert(i < passable.size());
    if (passable[i])
        return;
    passable[i] = true;
    auto v = i*4 + (uint32_t)from;
    fm_debug_assert(v < visited.size());
    if (visited[v])
        return;
    visited[v] = true;
    arrayAppend(stack, {pos});
}

Pointer<tmp_s> tmp_s::make()
{
    auto ptr = Pointer<tmp_s>{InPlace};
    arrayResize(ptr->stack, 0);
    arrayReserve(ptr->stack, TILE_COUNT);
    ptr->visited = {};
    ptr->passable = {};
    return ptr;
}

void tmp_s::clear()
{
    arrayResize(stack, 0);
    visited = {};
    passable = {};
}

void do_column(StringView name)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    text(name);
    ImGui::TableNextColumn();
}

struct region_test : base_test
{
    result_s result;
    pending_s pending;
    Pointer<tmp_s> tmp;

    tmp_s& get_tmp();
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

void region_test::do_region_extraction(world& w, chunk_coords_ coord)
{
    auto* c = w.at(coord);
    if (!c)
    {
        result.exists = false;
        pending.exists = false;
        return;
    }

    auto& tmp = get_tmp();
    const auto nbs = w.neighbors(coord);

    constexpr Vector2i fours[4] = { {0, 1}, {0, -1}, {1, 0}, {-1, 0} };
    constexpr auto last = div_count - Vector2i{1};
    //if (Vector2i pos{0, 0}; check_pos(*c, nbs, pos, fours[1])) tmp.append(pos, 1); // top

    for (Int i = 0; i < div_count.x(); i++)
    {
        if (Vector2i pos{i, last.y()}; check_pos(*c, nbs, pos, fours[0])) tmp.append(pos, 0); // bottom
        if (Vector2i pos{i, 0};        check_pos(*c, nbs, pos, fours[1])) tmp.append(pos, 1); // top
        if (Vector2i pos{last.x(), i}; check_pos(*c, nbs, pos, fours[2])) tmp.append(pos, 2); // right
        if (Vector2i pos{0, i};        check_pos(*c, nbs, pos, fours[3])) tmp.append(pos, 3); // left
    }

    while (!tmp.stack.isEmpty())
    {
        auto p = tmp.stack.back().pos;
        arrayRemoveSuffix(tmp.stack);
        for (int i = 0; i < 4; i++)
        {
            Vector2i from = fours[i], pos{p - from};
            if ((uint32_t)pos.x() >= div_count.x() || (uint32_t)pos.y() >= div_count.y()) [[unlikely]]
                continue;
            if (check_pos(*c, nbs, pos, from))
                tmp.append(pos, i);
        }
    }

    result = {
        .is_passable = tmp.passable,
        .c = coord,
        .exists = true,
    };
}

void region_test::draw_overlay(app& a)
{
    if (result.exists)
    {
        constexpr float dot_radius = 4;
        const auto dot_color = ImGui::ColorConvertFloat4ToU32({1, 0, 1, 1});
        ImDrawList& draw = *ImGui::GetForegroundDrawList();
        auto start = point{result.c, {0, 0}, {0, 0}};

        for (int j = 0; j < div_count.y(); j++)
            for (int i = 0; i < div_count.x(); i++)
            {
                auto index = (uint32_t)j * div_count.x() + (uint32_t)i;
                if (result.is_passable[index])
                    continue;
                auto pos = div_min + div_size * Vector2i{i, j};
                auto pt = object::normalize_coords(start, pos);
                auto px = a.point_screen_pos(pt);
                draw.AddCircleFilled({px.x(), px.y()}, dot_radius, dot_color);
            }
    }
}

void region_test::draw_ui(app&, float)
{
    if (!result.exists)
        return;

    char buf[128];
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr auto colflags_1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder |
                                ImGuiTableColumnFlags_NoSort;
    constexpr auto colflags_0 = colflags_1 | ImGuiTableColumnFlags_WidthFixed;

    if (auto b1 = begin_table("##region-results", 2, table_flags))
    {
        ImGui::TableSetupColumn("##name", colflags_0);
        ImGui::TableSetupColumn("##value", colflags_1 | ImGuiTableColumnFlags_WidthStretch);

        do_column("chunk");
        if (result.c.z != 0)
            std::snprintf(buf, sizeof buf, "%d x %d x %d", (int)result.c.x, (int)result.c.y, (int)result.c.z);
        else
            std::snprintf(buf, sizeof buf, "%d x %d", (int)result.c.x, (int)result.c.y);
        text(buf);

        do_column("passable");
        std::snprintf(buf, sizeof buf, "%zu", result.is_passable.count());
        //{ auto b = push_style_color(ImGuiCol_Text, 0x00ff00ff_rgbaf); text(buf); }
        text(buf);

        do_column("blocked");
        std::snprintf(buf, sizeof buf, "%zu", result.is_passable.size() - result.is_passable.count());
        //{ auto b = push_style_color(ImGuiCol_Text, 0xffff00ff_rgbaf); text(buf); }
        text(buf);
    }
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
    if (pending.exists)
    {
        pending.exists = false;
        auto& M = a.main();
        auto& w = M.world();
        do_region_extraction(w, pending.c);
    }
}

} // namespace

Pointer<base_test> tests_data::make_test_region() { return Pointer<region_test>{InPlaceInit}; }

} // namespace floormat::tests