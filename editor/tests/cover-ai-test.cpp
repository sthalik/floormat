#include "../tests-private.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "../raycast-draw.hpp"
#include "compat/array-size.hpp"
#include "compat/format.hpp"
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

enum class note : uint8_t { none, no_free_spot, no_start, start_taken, no_path, no_other_cover, };

// What update_spots found for a listed spot in the last frame: whether the player sees its center,
// and when not, find_cover_peek's spot for it.
struct spot_state
{
    cover_peek peek;
    bool seen = false, has_peek = false;
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
    void make_list();
    void update_spots(app& a, critter& c);
    bool player_sees(app& a, const critter& c, point pt);
    void send(critter& c, uint32_t k, bool peek);
    Optional<uint32_t> switch_cover(app& a);
    void update_rates(const cover_script_status& s);
    void update_ray(app& a, const critter& c, const cover_script_status& s);

    weak_bptr<critter> npc;
    rc::raycast_result_s ray;
    Array<cover_spot> spots;
    Array<spot_state> spot_states;
    cover_script_options options;
    point start_pos, list_cover;
    object_id threat = 0;
    Ns clock{}, snapshot_time{}, spots_time{};
    counts snapshot;
    per_second rates;
    note last_note = note::none;
    bool show_spots = false;
    bool has_start : 1 = false, has_snapshot : 1 = false, has_list : 1 = false, list_has_cover : 1 = false;
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
    arrayClear(spots);
    arrayClear(spot_states);
    has_list = false;
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

void cover_ai_test::make_list()
{
    arrayClear(spots);
    arrayClear(spot_states);
    has_list = true;
    list_has_cover = false;
    auto c = npc.lock();
    if (!c)
        return;
    if (const auto s = get_cover_script_status(*c))
    {
        list_has_cover = s->has_cover;
        list_cover = s->cover;
    }
    list_cover_spots(*c, spots);
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

// Runs every frame, since the player can move in any of them: one ray from the player to each
// listed spot, and find_cover_peek for the spots the ray doesn't reach.
void cover_ai_test::update_spots(app& a, critter& c)
{
    auto t = Time::now();
    const auto n = (uint32_t)spots.size();
    if (arrayCapacity(spot_states) < n)
        arrayReserve(spot_states, n + n/2);
    arrayClear(spot_states);
    for (const auto& x : spots)
    {
        spot_state st;
        st.seen = player_sees(a, c, x.pos);
        if (!st.seen)
            if (const auto p = find_cover_peek(c, x))
            {
                st.peek = *p;
                st.has_peek = true;
            }
        arrayAppend(spot_states, st);
    }
    spots_time = t.update();
}

void cover_ai_test::send(critter& c, uint32_t k, bool peek)
{
    const auto ok = peek ? send_to_peek(c, spots[k]) : send_to_cover(c, spots[k]);
    last_note = ok ? note::none : note::no_path;
}

// Lists the spots again and sends the npc to the first after its cover, wrapping around, that the
// player doesn't see and a path leads to. Returns its index in the list, or empty when there is
// none.
Optional<uint32_t> cover_ai_test::switch_cover(app& a)
{
    const auto c = npc.lock();
    const auto s = c ? get_cover_script_status(*c) : Optional<cover_script_status>{};
    if (!s)
        return {};
    make_list();
    const auto n = (uint32_t)spots.size();
    auto from = 0u;
    while (from < n && !(s->has_cover && spots[from].pos == s->cover))
        from++;
    for (auto j = 1u; j <= n; j++)
    {
        const auto k = (from + j) % n;
        if (k == from || player_sees(a, *c, spots[k].pos))
            continue;
        if (send_to_cover(*c, spots[k]))
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

// The spots are listed again only when the cover changes, since each listing is a whole
// find_cover_point search.
void cover_ai_test::update_post(app& a, const Ns& dt)
{
    clock += dt;
    const auto c = npc.lock();
    const auto s = c ? get_cover_script_status(*c) : Optional<cover_script_status>{};
    if (!s)
    {
        ray.has_result = false;
        arrayClear(spots);
        arrayClear(spot_states);
        has_list = false;
        has_snapshot = false;
        rates = {};
        return;
    }
    update_rates(*s);
    update_ray(a, *c, *s);
    if (show_spots && (!has_list || s->has_cover != list_has_cover || s->has_cover && s->cover != list_cover))
        make_list();
    if (show_spots)
        update_spots(a, *c);
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
        make_list();
        ret.type = ValueType::u32;
        ret.u32 = (uint32_t)spots.size();
        break;
    case 4:
        if (const auto k = switch_cover(a))
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
        // A list made after update_post, by a click or a button, has no states until the next one.
        const auto known = spot_states.size() == spots.size();
        for (auto i = 0u; i < spots.size(); i++)
        {
            const auto seen = known && spot_states[i].seen;
            draw_dot(bg, a, spots[i].pos, 3, seen ? ImVec4{0.6f, 0.6f, 0.6f, 0.8f} : ImVec4{0.3f, 0.7f, 1, 0.9f});
            if (known && spot_states[i].has_peek)
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
    if (ImGui::Button("Switch to other cover"))
        (void)switch_cover(a);

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

    if (ImGui::Checkbox("Other cover spots", &show_spots))
    {
        if (show_spots)
            make_list();
        else
            has_list = false;
    }
    if (show_spots)
        ImGui::TextWrapped("Click a blue dot to send the npc to hide there, or an orange one to peek from "
                           "its spot. The player sees the grey ones.");

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
        if (show_spots && has_list && spot_states.size() == spots.size())
        {
            auto seen = 0u, peeks = 0u;
            for (const auto& x : spot_states)
            {
                seen += x.seen;
                peeks += x.has_peek;
            }
            snformat(buf, "{}, {} seen, {} with peek spots", spots.size(), seen, peeks);
            text(buf);
            do_column("listed spots time");
            snformat(buf, "{:.1f} µs per frame", us(spots_time));
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
