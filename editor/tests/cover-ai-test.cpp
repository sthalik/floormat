#include "../tests-private.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "../raycast-draw.hpp"
#include "compat/array-size.hpp"
#include "compat/format.hpp"
#include "compat/fwd/algorithm/sort.hpp"
#include "compat/weak-borrowed-ptr.inl"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/critter-script-cover.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/nanosecond.inl"
#include "src/point.inl"
#include "src/raycast.hpp"
#include "src/search-astar.hpp"
#include "src/search-pred.hpp"
#include "src/tile-defs.hpp"
#include "src/timer.hpp"
#include "src/world.hpp"
#include "floormat/main.hpp"
#include <cr/GrowableArray.h>
#include <cr/Optional.h>

namespace floormat::tests {

namespace {

using namespace floormat::imgui;
using state = cover_script_status::state;

constexpr uint32_t cover_div = 8;
constexpr int spawn_distance = 4*tile_size_xy;
constexpr Vector2i spawn_dirs[] = { {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, };
constexpr uint8_t octant_choices[] = { 0, 1, 3, 5, 7, 9, };
constexpr const char* state_names[] = { "seek", "to cover", "hidden", "to peek", "exposed", "retreat", };
static_assert(array_size(state_names) == (size_t)state::retreat + 1);
// how far from a dot's center, in screen px, a click still picks it
constexpr float pick_radius = 6;
// The highest and default radius of the overlay's search, in tiles: 1024 px, as far as a cover grid
// cell sees blockers. pgo-driver-cover-ai.cpp's overlay_spots takes the same radius.
constexpr int max_spots_radius_tiles = 16;
// How long the overlay's search may run in a frame. It stops after the tile that reaches it.
constexpr Ns spots_budget = 2*Millisecond;

enum class note : uint8_t { none, no_free_spot, no_start, start_taken, no_path, no_other_cover, };

// find_cover_peek's spot for a listed spot, found when the spot was listed.
struct spot_state
{
    cover_peek peek;
    bool has_peek = false;
};

struct counts { uint32_t searches = 0, rays = 0, paths = 0; };
struct per_second { float searches = 0, rays = 0, paths = 0; };

void print_coord(auto&& buf, point pt)
{
    const auto c = Vector3i(pt.chunk3());
    const auto l = Vector2i(pt.local());
    const auto o = Vector2i(pt.offset());
    snformat(buf, "(ch {}x{}) <{}x{}> {{{}x{} px}}", c.x(), c.y(), l.x(), l.y(), o.x(), o.y());
}

void do_column(StringView name)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    text(name);
    ImGui::TableNextColumn();
}

void draw_dot(ImDrawList& draw, app& a, point pt, float radius, ImVec4 color)
{
    const auto px = a.point_to_pixel(pt);
    draw.AddCircleFilled({px.x(), px.y()}, radius, ImGui::ColorConvertFloat4ToU32(color));
}

critter_proto npc_proto(point at)
{
    critter_proto p;
    p.name = "npc"_s;
    p.offset = at.offset();
    p.bbox_size = {24, 24};
    return p;
}

bool can_spawn_at(world& w, point pt)
{
    auto* c = w.at(pt.chunk3());
    return c && c->can_place_object(npc_proto(pt), pt.local());
}

struct cover_ai_test final : base_test
{
    ~cover_ai_test() noexcept override;

    bool handle_key(app&, const key_event&, bool) override { return false; }
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app&, const mouse_move_event&) override { return false; }
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float width) override;
    void update_pre(app&, const Ns&) override {}
    void update_post(app& a, const Ns& dt) override;
    Value advance(app& a, Value v) override;
    void on_leave_tests_mode(app&) override { stop(); }

    object_id start(app& a);
    object_id reset(app& a);
    void stop();
    object_id spawn(app& a, point at);
    void apply_options();
    void clear_spots();
    bool spots_stale(app& a) const;
    void restart_spots(app& a, const critter& c);
    void search_spots(critter& c, Optional<Ns> budget);
    void search_tile(critter& c);
    bool player_sees(app& a, const critter& c, point pt);
    void send(critter& c, uint32_t k, bool peek);
    ArrayView<const cover_spot> script_spots(app& a, critter& c, const cover_script_status& s);
    Optional<uint32_t> step_cover(app& a, bool back);
    void draw_cover_picker(app& a);
    void update_rates(const cover_script_status& s);
    void update_ray(app& a, const critter& c, const cover_script_status& s);

    weak_bptr<critter> npc;
    rc::raycast_result_s ray;
    Array<cover_spot> spots, tile_spots, script_list;
    Array<spot_state> spot_states;
    // The tiles of the overlay's search, as offsets from search_center, nearest first.
    Array<Vector2i> tiles;
    cover_script_options options;
    point start_pos, search_center, search_threat;
    object_id threat = 0, script_list_npc = 0;
    uint64_t search_pass_gen = 0, script_list_pass_gen = 0;
    uint32_t next_tile = 0, search_radius = 0, tiles_radius = 0, script_list_searches = 0;
    Ns clock{}, snapshot_time{}, spots_time{}, search_time{};
    counts snapshot;
    per_second rates;
    int radius_tiles = max_spots_radius_tiles;
    note last_note = note::none;
    uint8_t search_octants = 0;
    bool show_spots = false;
    bool has_start : 1 = false, has_snapshot : 1 = false, has_search : 1 = false, has_script_list : 1 = false;
};

// reset_world_pre() runs finish_scripts() before tests_reset_mode() destroys the test, and
// kill_object() would then run do_destroy_pre(), which asserts that the script is still created.
// The world is replaced right after, and the npc with it.
cover_ai_test::~cover_ai_test() noexcept
{
    if (auto c = npc.lock(); c && !c->world().script_status().finalized)
        c->chunk().kill_object(*c, c->index());
}

void cover_ai_test::stop()
{
    if (auto c = npc.lock())
        c->chunk().kill_object(*c, c->index());
    npc = nullptr;
    ray.has_result = false;
    clear_spots();
}

// The npc's id, or 0 when no spot 4 tiles from the player is free.
object_id cover_ai_test::start(app& a)
{
    stop();
    auto& w = a.main().world();
    const auto from = a.ensure_player_character(w)->position();
    has_start = false;
    for (const auto d : spawn_dirs)
    {
        const auto pt = from + d*spawn_distance;
        if (can_spawn_at(w, pt))
        {
            start_pos = pt;
            has_start = true;
            return spawn(a, pt);
        }
    }
    last_note = note::no_free_spot;
    return 0;
}

// The new npc's id, or 0 when there was no start or its spot is no longer free.
object_id cover_ai_test::reset(app& a)
{
    stop();
    if (!has_start)
    {
        last_note = note::no_start;
        return 0;
    }
    if (!can_spawn_at(a.main().world(), start_pos))
    {
        last_note = note::start_taken;
        return 0;
    }
    return spawn(a, start_pos);
}

object_id cover_ai_test::spawn(app& a, point at)
{
    auto& m = a.main();
    auto& w = m.world();
    threat = a.ensure_player_character(w)->id;
    auto c = w.make_object<critter>(w.make_id(), at.coord(), npc_proto(at));
    // serialize/savegame.cpp leaves it out of saves
    c->ephemeral = true;
    c->script.do_reassign(critter_script::make_cover_script(m.astar(), threat, cover_div), c);
    set_cover_script_options(*c, options);
    npc = c;
    has_snapshot = false;
    rates = {};
    last_note = note::none;
    return c->id;
}

void cover_ai_test::apply_options()
{
    if (auto c = npc.lock())
        set_cover_script_options(*c, options);
}

void cover_ai_test::clear_spots()
{
    arrayClear(spots);
    arrayClear(spot_states);
    has_search = false;
}

// The overlay's spots are hidden from where the player stood when its search started, with the
// colliders as they were then. world::pass_gen() moves when a chunk's walls, ground, holes or static
// objects change, or a chunk is made or destroyed, but critters walking don't move it.
bool cover_ai_test::spots_stale(app& a) const
{
    if (!has_search)
        return true;
    auto& w = a.main().world();
    const auto player = w.find_object(threat);
    return !player || player->position() != search_threat || w.pass_gen() != search_pass_gen ||
           search_radius != (uint32_t)radius_tiles*tile_size_xy || search_octants != options.octants;
}

// Starts the overlay's search over, around the center of the tile c stands on, so that each tile
// it searches is a tile of the world.
void cover_ai_test::restart_spots(app& a, const critter& c)
{
    clear_spots();
    auto& w = a.main().world();
    const auto player = w.find_object(threat);
    if (!player)
        return;
    const auto r = (uint32_t)radius_tiles*tile_size_xy;
    if (r != tiles_radius)
    {
        arrayClear(tiles);
        const auto n = (int)r/tile_size_xy + 1;
        for (auto y = -n; y <= n; y++)
            for (auto x = -n; x <= n; x++)
            {
                const auto gap = Math::max(Math::abs(Vector2i{x, y}*tile_size_xy) - Vector2i{half_tile<int>},
                                           Vector2i{});
                if (gap.dot() <= (int)(r*r))
                    arrayAppend(tiles, Vector2i{x, y});
            }
        std::sort(tiles.begin(), tiles.end(), [](Vector2i a, Vector2i b) {
            if (a.dot() != b.dot())
                return a.dot() < b.dot();
            return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
        });
        tiles_radius = r;
    }
    search_center = point{c.position().coord(), {}};
    search_threat = player->position();
    search_pass_gen = w.pass_gen();
    search_radius = r;
    search_octants = options.octants;
    next_tile = 0;
    search_time = Ns{};
    has_search = true;
}

// Lists the spots of the next tile that no earlier tile listed, with their peek spots. A candidate
// in one tile can be pushed onto a spot in another, so two tiles can list one spot.
void cover_ai_test::search_tile(critter& c)
{
    const auto mid = tiles[next_tile++]*tile_size_xy;
    const cover_query q{search_center, Vector2ui(c.bbox_size), search_threat, search_radius, search_octants,
                        Range2Di{mid - Vector2i{half_tile<int>}, mid + Vector2i{half_tile<int>}}};
    list_cover_spots(c, q, tile_spots);
    for (const auto& x : tile_spots)
    {
        auto i = 0u;
        while (i < spots.size() && spots[i].pos != x.pos)
            i++;
        if (i < spots.size())
            continue;
        spot_state st;
        if (const auto p = find_cover_peek(c, x))
        {
            st.peek = *p;
            st.has_peek = true;
        }
        arrayAppend(spots, x);
        arrayAppend(spot_states, st);
    }
}

void cover_ai_test::search_spots(critter& c, Optional<Ns> budget)
{
    auto t = Time::now();
    spots_time = Ns{};
    while (has_search && next_tile < tiles.size() && (!budget || spots_time < *budget))
    {
        search_tile(c);
        spots_time += t.update();
    }
    search_time += spots_time;
}

// The ray the script's check_hidden casts, from the player to pt.
bool cover_ai_test::player_sees(app& a, const critter& c, point pt)
{
    auto& m = a.main();
    const auto player = m.world().find_object(threat);
    if (!player)
        return false;
    auto& pool = m.astar().pool_registry().pass_pool(Pass::Params{cover_div, pass_mode::shoot_through});
    return raycast(m.world(), player->position(), pt, c.id, pool, Search::without_critters()).success;
}

void cover_ai_test::send(critter& c, uint32_t k, bool peek)
{
    const auto ok = peek ? send_to_peek(c, spots[k]) : send_to_cover(c, spots[k]);
    last_note = ok ? note::none : note::no_path;
}

// The spots of the script's last search, apart from the overlay's, from list_cover_spots(), which
// runs that search again. They are kept until the script searches again, world::pass_gen() moves or
// the npc is another, since listing them each frame would cost a whole search per frame. Critters
// walking don't move pass_gen, so a spot the player walks onto stays listed.
ArrayView<const cover_spot> cover_ai_test::script_spots(app& a, critter& c, const cover_script_status& s)
{
    const auto gen = a.main().world().pass_gen();
    if (!has_script_list || script_list_npc != c.id || script_list_searches != s.searches || script_list_pass_gen != gen)
    {
        list_cover_spots(c, script_list);
        script_list_npc = c.id;
        script_list_searches = s.searches;
        script_list_pass_gen = gen;
        has_script_list = true;
    }
    return script_list;
}

// Sends the npc to hide at the first of script_spots() after its cover, or before it when back is
// set, wrapping around, that the player doesn't see and a path leads to. When its cover isn't
// listed, the search starts at the list's first spot, or its last when back is set. Returns the
// spot's index in that list, or empty when there is none.
Optional<uint32_t> cover_ai_test::step_cover(app& a, bool back)
{
    const auto c = npc.lock();
    const auto s = c ? get_cover_script_status(*c) : Optional<cover_script_status>{};
    if (!s)
        return {};
    const auto list = script_spots(a, *c, *s);
    const auto n = (uint32_t)list.size();
    auto from = 0u;
    while (from < n && !(s->has_cover && list[from].pos == s->cover))
        from++;
    const auto listed = from < n;
    if (!listed)
        from = back ? 0 : n - 1;
    for (auto j = 0u; j < n; j++)
    {
        const auto k = back ? (from + n - 1 - j) % n : (from + 1 + j) % n;
        if (listed && k == from || player_sees(a, *c, list[k].pos))
            continue;
        if (send_to_cover(*c, list[k]))
        {
            last_note = note::none;
            return k;
        }
    }
    last_note = note::no_other_cover;
    return {};
}

// A left click picks the nearest dot of the overlay within pick_radius. A listed spot's dot sends
// the npc to hide there, and a peek spot's dot sends it to peek from a spot it belongs to. On a
// tie the listed spot wins. Spots along one face of a blocker often slide to the same peek spot,
// and then the npc's cover is the one it peeks from when it is among them, else the one nearest
// the peek spot, which has the shortest walk back.
bool cover_ai_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    if (is_down || e.button != mouse_button_left || !show_spots)
        return false;
    const auto c = npc.lock();
    const auto s = c ? get_cover_script_status(*c) : Optional<cover_script_status>{};
    if (!s)
        return false;
    auto best = pick_radius*pick_radius;
    auto k = (uint32_t)-1;
    auto peek = false;
    for (auto i = 0u; i < spots.size(); i++)
        if (const auto d = (a.point_to_pixel(spots[i].pos) - e.position).dot(); d < best)
        {
            best = d;
            k = i;
        }
    for (auto i = 0u; i < spot_states.size(); i++)
        if (spot_states[i].has_peek)
            if (const auto d = (a.point_to_pixel(spot_states[i].peek.pos) - e.position).dot(); d < best)
            {
                best = d;
                k = i;
                peek = true;
            }
    if (k == (uint32_t)-1)
        return false;
    if (peek)
    {
        const auto pt = spot_states[k].peek.pos;
        for (auto i = 0u; i < spot_states.size(); i++)
        {
            if (i == k || !spot_states[i].has_peek || spot_states[i].peek.pos != pt)
                continue;
            if (s->has_cover && spots[k].pos == s->cover)
                break;
            if (s->has_cover && spots[i].pos == s->cover ||
                point::distance_squared(spots[i].pos, pt) < point::distance_squared(spots[k].pos, pt))
                k = i;
        }
    }
    send(*c, k, peek);
    return true;
}

void cover_ai_test::update_rates(const cover_script_status& s)
{
    const counts now{s.searches, s.rays, s.paths};
    if (!has_snapshot)
    {
        snapshot = now;
        snapshot_time = clock;
        has_snapshot = true;
        return;
    }
    const auto dt = clock - snapshot_time;
    if (dt < Second)
        return;
    const auto secs = Time::to_seconds(dt);
    rates = {
        .searches = (float)(now.searches - snapshot.searches) / secs,
        .rays = (float)(now.rays - snapshot.rays) / secs,
        .paths = (float)(now.paths - snapshot.paths) / secs,
    };
    snapshot = now;
    snapshot_time = clock;
}

// Casts the status's ray from gun_seen_from to gun again, with the script's pool and filter, so the
// drawn line and the "ray" row show a ray the script found clear, at grazing angles too, unless the
// world changed since. The line starts at the player whenever the script checked the gun after the
// player's last move, and otherwise where the player stood at the last check.
void cover_ai_test::update_ray(app& a, const critter& c, const cover_script_status& s)
{
    ray.has_result = false;
    if (s.st != state::exposed || !s.has_peek)
        return;
    auto& m = a.main();
    auto& pool = m.astar().pool_registry().pass_pool(Pass::Params{cover_div, pass_mode::shoot_through});
    ray = raycast(m.world(), s.gun_seen_from, s.gun, c.id, pool, Search::without_critters());
}

void cover_ai_test::update_post(app& a, const Ns& dt)
{
    clock += dt;
    const auto c = npc.lock();
    const auto s = c ? get_cover_script_status(*c) : Optional<cover_script_status>{};
    if (!s)
    {
        ray.has_result = false;
        clear_spots();
        has_snapshot = false;
        rates = {};
        return;
    }
    update_rates(*s);
    update_ray(a, *c, *s);
    if (!show_spots)
        return;
    if (spots_stale(a))
        restart_spots(a, *c);
    search_spots(*c, spots_budget);
}

base_test::Value cover_ai_test::advance(app& a, Value v)
{
    Value ret;
    switch (v.u32)
    {
    case 0:
    case 3:
        if (const auto id = v.u32 == 0 ? start(a) : reset(a))
        {
            ret.type = ValueType::u64;
            ret.u64 = id;
        }
        break;
    case 1:
        stop();
        break;
    case 2:
        show_spots = true;
        if (const auto c = npc.lock())
        {
            restart_spots(a, *c);
            search_spots(*c, {});
        }
        ret.type = ValueType::u32;
        ret.u32 = (uint32_t)spots.size();
        break;
    case 4:
    case 5:
        if (const auto k = step_cover(a, v.u32 == 5))
        {
            ret.type = ValueType::u32;
            ret.u32 = *k;
        }
        break;
    default:
        break;
    }
    return ret;
}

void cover_ai_test::draw_overlay(app& a)
{
    if (ray.has_result)
        draw_raycast_line(a, ray);
    if (show_spots)
    {
        ImDrawList& bg = *ImGui::GetBackgroundDrawList();
        for (auto i = 0u; i < spots.size(); i++)
        {
            draw_dot(bg, a, spots[i].pos, 3, {0.3f, 0.7f, 1, 0.9f});
            if (spot_states[i].has_peek)
                draw_dot(bg, a, spot_states[i].peek.pos, 2.5f, {1, 0.55f, 0.15f, 0.9f});
        }
    }
    const auto c = npc.lock();
    if (!c)
        return;
    const auto s = get_cover_script_status(*c);
    if (!s)
        return;
    ImDrawList& fg = *ImGui::GetForegroundDrawList();
    if (s->has_cover)
        draw_dot(fg, a, s->cover, 6, {0.2f, 1, 0.2f, 1});
    // The last find_peek result. While a flanked npc still peeks, it belongs to the cover it peeks
    // from, and s->cover is already the one it will hide at.
    if (s->has_peek)
    {
        draw_dot(fg, a, s->peek, 5, {1, 0.85f, 0, 1});
        draw_dot(fg, a, s->gun, 4, {1, 0.2f, 0.2f, 1});
    }
}

void cover_ai_test::draw_cover_picker(app& a)
{
    char preview[96], label[96], coord[64];
    const auto c = npc.lock();
    auto s = c ? get_cover_script_status(*c) : Optional<cover_script_status>{};
    auto d = begin_disabled(!s);

    if (ImGui::ArrowButton("##prev-cover", ImGuiDir_Left))
    {
        (void)step_cover(a, true);
        s = get_cover_script_status(*c);
    }
    ImGui::SameLine();

    ArrayView<const cover_spot> list;
    if (s)
        list = script_spots(a, *c, *s);
    auto cur = (uint32_t)-1;
    for (auto i = 0u; i < list.size(); i++)
        if (s->has_cover && list[i].pos == s->cover)
        {
            cur = i;
            break;
        }
    if (list.isEmpty())
        snformat(preview, "no spots listed");
    else if (cur == (uint32_t)-1)
        snformat(preview, "cover not among {} spots", list.size());
    else
    {
        print_coord(coord, list[cur].pos);
        snformat(preview, "{}/{}: {}", cur + 1, list.size(), (const char*)coord);
    }
    ImGui::SetNextItemWidth(-(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x));
    if (auto b = begin_combo("##cover-spot", preview))
        for (auto i = 0u; i < list.size(); i++)
        {
            print_coord(coord, list[i].pos);
            snformat(label, "{}: {}", i + 1, (const char*)coord);
            if (ImGui::Selectable(label, i == cur))
                last_note = send_to_cover(*c, list[i]) ? note::none : note::no_path;
        }
    ImGui::SameLine();

    if (ImGui::ArrowButton("##next-cover", ImGuiDir_Right))
        (void)step_cover(a, false);
}

void cover_ai_test::draw_ui(app& a, float)
{
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    constexpr auto col1 = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort;
    constexpr auto col0 = col1 | ImGuiTableColumnFlags_WidthFixed;

    char buf[128], buf2[96];

    ImGui::NewLine();
    if (ImGui::Button("Start"))
        start(a);
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
        stop();
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
        reset(a);
    draw_cover_picker(a);

    switch (last_note)
    {
    case note::none:
        break;
    case note::no_free_spot:
        text("No free spot 4 tiles from the player.");
        break;
    case note::no_start:
        text("Nothing to reset: Start first.");
        break;
    case note::start_taken:
        text("The spot of the last Start is no longer free.");
        break;
    case note::no_path:
        text("No path leads there.");
        break;
    case note::no_other_cover:
        text("No other listed spot is hidden from the player and has a path.");
        break;
    }

    bool only_seen = !options.search_every_move;
    if (ImGui::Checkbox("Search only when seen", &only_seen))
    {
        options.search_every_move = !only_seen;
        apply_options();
    }

    int k = 0;
    while (k + 1 < (int)array_size(octant_choices) && octant_choices[k] != options.octants)
        k++;
    if (octant_choices[k])
        snformat(buf, "{}", octant_choices[k]);
    else
        snformat(buf, "off");
    constexpr auto slider_flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput;
    if (ImGui::SliderInt("Octants checked", &k, 0, (int)array_size(octant_choices) - 1, buf, slider_flags))
    {
        options.octants = octant_choices[k];
        apply_options();
    }

    if (ImGui::Checkbox("Other cover spots", &show_spots) && !show_spots)
        clear_spots();
    if (show_spots)
    {
        snformat(buf, "{} tiles, {} px", radius_tiles, radius_tiles*tile_size_xy);
        ImGui::SliderInt("Spots radius", &radius_tiles, 1, max_spots_radius_tiles, buf, slider_flags);
        ImGui::TextWrapped("Spots hidden from where the player stands, searched a few tiles per frame, "
                           "starting over when the player moves or a wall changes. Click a blue dot to send "
                           "the npc to hide there, or an orange one to peek from its spot.");
    }

    const auto c = npc.lock();
    const auto s = c ? get_cover_script_status(*c) : Optional<cover_script_status>{};
    auto& w = a.main().world();
    const auto player = threat ? w.find_object(threat) : bptr<object>{};

    if (auto b = begin_table("##cover-ai", 2, table_flags))
    {
        ImGui::TableSetupColumn("##name", col0);
        ImGui::TableSetupColumn("##value", col1 | ImGuiTableColumnFlags_WidthStretch);

        const auto coord_or_dash = [&](bool known, point pt) {
            if (!known)
                return text("-");
            print_coord(buf, pt);
            text(buf);
        };

        do_column("state");
        text(!c ? "no npc" : !s ? "no cover script" : state_names[(size_t)s->st]);

        do_column("npc");
        if (c)
        {
            print_coord(buf2, c->position());
            snformat(buf, "{} {}", (uint64_t)c->id, (const char*)buf2);
            text(buf);
        }
        else
            text("-");

        do_column("cover");
        coord_or_dash(s && s->has_cover, s ? s->cover : point{});
        do_column("last peek spot");
        coord_or_dash(s && s->has_peek, s ? s->peek : point{});
        do_column("last gun point");
        coord_or_dash(s && s->has_peek, s ? s->gun : point{});
        do_column("player");
        coord_or_dash(!!player, player ? player->position() : point{});

        do_column("ray");
        if (!ray.has_result)
            text("-");
        else if (ray.success)
            text("seen");
        else
        {
            print_coord(buf2, ray.collision);
            snformat(buf, "hit at {}", (const char*)buf2);
            text(buf);
        }

        const auto us = [](const Ns& t) { return (double)(uint64_t)t * 1e-3; };

        do_column("listed spots");
        if (show_spots && has_search)
        {
            auto peeks = 0u;
            for (const auto& x : spot_states)
                peeks += x.has_peek;
            snformat(buf, "{}, {} with peek spots", spots.size(), peeks);
            text(buf);
            do_column("tiles searched");
            snformat(buf, "{} of {}", next_tile, tiles.size());
            text(buf);
            do_column("search time");
            snformat(buf, "{:.1f} µs this frame, {:.1f} µs in all", us(spots_time), us(search_time));
            text(buf);
        }
        else
            text("-");

        do_column("update time");
        if (s)
        {
            snformat(buf, "{:.1f} µs, highest {:.1f} µs", us(s->last_update), us(s->max_update));
            text(buf);
        }
        else
            text("-");

        const auto rate_row = [&](StringView name, float value) {
            do_column(name);
            if (!s)
                return text("-");
            snformat(buf, "{:.1f}", (double)value);
            text(buf);
        };
        rate_row("searches/s", rates.searches);
        rate_row("rays/s", rates.rays);
        rate_row("paths/s", rates.paths);
    }
}

} // namespace

Pointer<base_test> tests_data::make_test_cover_ai() { return Pointer<cover_ai_test>{InPlaceInit}; }

} // namespace floormat::tests
