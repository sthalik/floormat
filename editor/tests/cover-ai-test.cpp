#include "../tests-private.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "../raycast-draw.hpp"
#include "compat/array-size.hpp"
#include "compat/format.inl"
#include "compat/weak-borrowed-ptr.inl"
#include "compat/nanosecond.inl"
#include "compat/timer.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/critter-script-cover.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-registry.hpp"
#include "src/point.inl"
#include "src/raycast.hpp"
#include "src/search-astar.hpp"
#include "src/search-pred.hpp"
#include "src/tile-defs.hpp"
#include "src/world.hpp"
#include "floormat/main.hpp"
#include <bit>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>

namespace floormat::tests {

namespace {

using namespace floormat::imgui;
using state = Cover::Script::status::state;
using hold_phase = Cover::Script::options::hold_phase;

constexpr int spawn_distance = 4*tile_size_xy;
constexpr Vector2i spawn_dirs[] = { {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, };
constexpr u8 octant_choices[] = { 0, 1, 3, 5, 7, 9, };
constexpr u32 div_choices[] = { 2, 4, 8, 16, 32, 64, };
constexpr const char* state_names[] = { "seek", "to cover", "hidden", "to peek", "exposed", "retreat", "search", "find retreat", };
static_assert(array_size(state_names) == (usize)state::find_retreat + 1);
// how far from a dot's center, in screen px, a click still picks it
constexpr float pick_radius = 6;
// The default radius of the overlay's search, in tiles: 1024 px, as far as a cover grid cell sees
// blockers. pgo-driver-cover-ai.cpp's overlay_spots takes the same radius. The slider doubles it
// from 1 tile up to 1 << max_spots_radius_log2 tiles.
constexpr int default_spots_radius_tiles = 16, max_spots_radius_log2 = 6;
// The overlay's search per frame, as the script's search per update in critter-script-cover.cpp. A
// tile whose listing needs another fill waits for the next frame.
constexpr u32 spots_tiles_per_frame = 32, spots_fills_per_frame = 1;

enum class note : u8 { none, no_free_spot, no_start, start_taken, no_path, no_other_cover, };

// Cover::Script::find_peek's spot for a listed spot, found when the spot was listed.
struct spot_state
{
    Cover::Script::peek peek;
    bool has_peek = false;
};

struct counts { u32 num_searches = 0, num_rays = 0, num_dijkstra_calls = 0; };
struct per_second { float num_searches = 0, num_rays = 0, num_dijkstra_calls = 0; };

void print_coord(auto&& buf, point pt)
{
    const auto c = Vector3i(pt.chunk3());
    const auto l = Vector2i(pt.local());
    const auto o = Vector2i(pt.offset());
    snformat(buf, "(ch {}x{}) <{}x{}> {{{}x{} px}}", c.x(), c.y(), l.x(), l.y(), o.x(), o.y());
}

void print_spot(auto&& buf, bool has, point pt)
{
    const auto v = Vector3i(pt).xy();
    if (has)
        snformat(buf, "({}, {})", v.x(), v.y());
    else
        snformat(buf, "none");
}

double us(const Ns& t) { return (double)(u64)t * 1e-3; }

// Octant fills of the cover pool the npc's script searches with, and their time, since the pool
// was made. A search fills the octants it reads in each chunk the first time.
struct fill_count { u32 n = 0; Ns time{}; };

fill_count fills_of(app& a, u32 div_size)
{
    auto& p = a.main().astar().pool_registry().cover_pool(Grid::Cover::Params{div_size});
    return {p.octant_fills(), p.octant_fill_time()};
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
    draw.AddCircleFilled(ImVec2(a.point_to_pixel(pt)), radius, ImGui::ColorConvertFloat4ToU32(color));
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
    cover_ai_test();
    ~cover_ai_test() noexcept override;

    bool handle_key(app&, const key_event&, bool) override { return false; }
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app&, const mouse_move_event&) override { return false; }
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float width) override;
    void update_pre(app& a, const Ns&) override;
    void update_post(app& a, const Ns& dt) override;
    Value advance(app& a, Value v) override;
    void on_leave_tests_mode(app&) override { stop(); }

    object_id start(app& a);
    object_id reset(app& a);
    void stop();
    object_id spawn(app& a, point at);
    void apply_options();
    void set_div(app& a, u32 div_size);
    fill_count fills(app& a) const { return fills_of(a, options.div_size); }
    void clear_spots();
    bool spots_stale(app& a) const;
    void restart_spots(app& a, const critter& c);
    void drop_seen_spots(app& a, const critter& c);
    u32 seen_spots(app& a, const critter& c);
    void search_spots(app& a, critter& c, bool limited);
    Optional<u32> search_tile(app& a, critter& c, point player, u32 max_fills);
    bool player_sees(app& a, const critter& c, const Cover::Point::spot& s);
    bool send(critter& c, const Cover::Point::spot& s, bool peek);
    u32 toggle_hold();
    u32 toggle_move_to_end();
    ArrayView<const Cover::Point::spot> script_spots(app& a, critter& c, const Cover::Script::status& s);
    Optional<u32> step_cover(app& a, bool back);
    void draw_cover_picker(app& a);
    void update_rates(const Cover::Script::status& s);
    void update_ray(app& a, const critter& c, const Cover::Script::status& s);
    void log_script(app& a, const critter& c, const Cover::Script::status& s);

    weak_bptr<critter> npc;
    rc::raycast_result_s ray;
    Cover::Point::finder finder;
    Array<Cover::Point::spot> spots, tile_spots, script_list;
    Array<spot_state> spot_states;
    // The tiles of the overlay's search, as offsets from search_center, nearest first.
    Array<Vector2i> tiles;
    Cover::Script::options options;
    // spots_player: where the player stood when drop_seen_spots last ran, or the search started.
    point start_pos, search_center, spots_player;
    object_id threat = 0, script_list_npc = 0;
    u64 search_pass_gen = 0, script_list_pass_gen = 0;
    u32 next_tile = 0, search_radius = 0, search_listed_cover_end_dist = 0, tiles_radius = 0;
    u32 search_max_cover_end_dist = 0, search_threat_reach = 0;
    u32 search_octant = 0, search_div = 0, dropped_spots = 0, script_list_num_searches = 0;
    Ns clock{}, snapshot_time{}, spots_time{}, search_time{};
    fill_count fills_before_update, search_fills;
    Cover::Script::status logged;
    object_id logged_npc = 0;
    counts snapshot;
    per_second rates;
    int radius_tiles = default_spots_radius_tiles;
    note last_note = note::none;
    u8 search_octants = 0;
    bool show_spots = false;
    bool has_start : 1 = false, has_snapshot : 1 = false, has_search : 1 = false, has_script_list : 1 = false;
};

cover_ai_test::cover_ai_test()
{
    arrayReserve(spots, 16);
    arrayReserve(spot_states, 16);
}

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
    c->script.do_reassign(critter_script::make_cover_script(m.astar(), threat, options.div_size), c);
    Cover::Script::set_options(*c, options);
    npc = c;
    has_snapshot = false;
    rates = {};
    last_note = note::none;
    return c->id;
}

void cover_ai_test::apply_options()
{
    if (auto c = npc.lock())
        Cover::Script::set_options(*c, options);
}

// Also drops script_spots()' list, since the script's last query accepts other spots in grids of
// another div_size.
void cover_ai_test::set_div(app& a, u32 div_size)
{
    options.div_size = div_size;
    apply_options();
    has_script_list = false;
    fills_before_update = fills(a);
}

void cover_ai_test::clear_spots()
{
    arrayClear(spots);
    arrayClear(spot_states);
    has_search = false;
}

// As in the script, only a player move to another octant starts the search over. drop_seen_spots handles the moves
// within one, and walking critters don't move world::pass_gen().
bool cover_ai_test::spots_stale(app& a) const
{
    if (!has_search)
        return true;
    auto& w = a.main().world();
    const auto player = w.find_object(threat);
    return !player || Cover::Point::nearest_octant(Vector2(player->position() - search_center)) != search_octant ||
           w.pass_gen() != search_pass_gen || search_radius != (u32)radius_tiles*tile_size_xy ||
           search_octants != options.octants || search_listed_cover_end_dist != options.listed_cover_end_dist() ||
           search_max_cover_end_dist != options.max_cover_end_dist ||
           search_threat_reach != options.threat_reach || search_div != options.div_size;
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
    const auto r = (u32)radius_tiles*tile_size_xy;
    if (r != tiles_radius)
    {
        Cover::Point::nearest_tiles(r, tiles);
        tiles_radius = r;
    }
    search_center = point{c.position().coord(), {}};
    spots_player = player->position();
    dropped_spots = 0;
    search_octant = Cover::Point::nearest_octant(Vector2(player->position() - search_center));
    search_pass_gen = w.pass_gen();
    search_radius = r;
    search_octants = options.octants;
    search_listed_cover_end_dist = options.listed_cover_end_dist();
    search_max_cover_end_dist = options.max_cover_end_dist;
    search_threat_reach = options.threat_reach;
    search_div = options.div_size;
    next_tile = 0;
    search_time = Ns{};
    search_fills = {};
    has_search = true;
}

// After a player move within the search's octant, drops the listed spots the player sees by
// player_sees(). A spot hidden again comes back only when the search starts over.
void cover_ai_test::drop_seen_spots(app& a, const critter& c)
{
    const auto player = a.main().world().find_object(threat);
    if (!player || player->position() == spots_player)
        return;
    spots_player = player->position();
    auto n = 0u;
    for (auto i = 0u; i < spots.size(); i++)
        if (!player_sees(a, c, spots[i]))
        {
            spots[n] = spots[i];
            spot_states[n++] = spot_states[i];
        }
    dropped_spots += (u32)spots.size() - n;
    arrayRemoveSuffix(spots, spots.size() - n);
    arrayRemoveSuffix(spot_states, spot_states.size() - n);
}

// The listed spots the player sees now.
u32 cover_ai_test::seen_spots(app& a, const critter& c)
{
    auto n = 0u;
    for (const auto& x : spots)
        n += player_sees(a, c, x);
    return n;
}

// Lists the next tile's spots that no earlier tile listed, as two tiles can list one spot, with their peek spots.
// Returns its octant fills, or empty, with the tile left for the next call, when it needs more than max_fills.
Optional<u32> cover_ai_test::search_tile(app& a, critter& c, point player, u32 max_fills)
{
    const auto f0 = fills(a);
    auto t = Time::now();
    const auto mid = tiles[next_tile]*tile_size_xy;
    const Cover::Point::query q{search_center, Vector2ui(c.bbox_size), player, search_radius, search_octants,
                        search_listed_cover_end_dist,
                        Range2Di{mid - Vector2i{half_tile<int>}, mid + Vector2i{half_tile<int>}},
                        max_fills};
    const auto r = Cover::Script::list_spots(c, q, tile_spots);
    const auto f = fills(a);
    search_fills.n += f.n - f0.n;
    search_fills.time += f.time - f0.time;
    if (r.out_of_fills)
        return {};
    next_tile++;
    auto added = 0u, peeks = 0u;
    for (const auto& x : tile_spots)
    {
        auto i = 0u;
        while (i < spots.size() && spots[i].pos != x.pos)
            i++;
        if (i < spots.size())
            continue;
        spot_state st;
        if (const auto p = Cover::Script::find_peek(c, x))
        {
            st.peek = *p;
            st.has_peek = true;
            peeks++;
        }
        arrayAppend(spots, x);
        arrayAppend(spot_states, st);
        added++;
    }
    const auto dt = t.update();
    if (added || f.n != f0.n)
    {
        const auto v = Vector3i(search_center).xy() + mid;
        fm_debug("cover-ai: spots tile (%d, %d): %u new, %u with peek spots, %.1f us, %u octant fills in %.1f us",
                 v.x(), v.y(), added, peeks, us(dt), f.n - f0.n, us(f.time - f0.time));
    }
    return r.fills;
}

// Without limited, lists every tile left, whatever the fills.
void cover_ai_test::search_spots(app& a, critter& c, bool limited)
{
    const auto player = a.main().world().find_object(threat);
    if (!player)
        return;
    auto t = Time::now();
    const auto first_tile = next_tile;
    auto fills_left = limited ? spots_fills_per_frame : (u32)-1;
    for (auto n = 0u; has_search && next_tile < tiles.size() && (!limited || n < spots_tiles_per_frame); n++)
    {
        const auto f = search_tile(a, c, player->position(), fills_left);
        if (!f)
            break;
        fills_left -= *f;
    }
    spots_time = t.update();
    search_time += spots_time;
    if (next_tile > first_tile && next_tile == tiles.size())
        fm_debug("cover-ai: spots search done: %u tiles, %u spots, %.1f ms, %u octant fills in %.1f ms",
                 (u32)tiles.size(), (u32)spots.size(), us(search_time)*1e-3, search_fills.n,
                 us(search_fills.time)*1e-3);
}

// True when the player sees a pixel of c's body at s, as the script's check_hidden tests canonical.
bool cover_ai_test::player_sees(app& a, const critter& c, const Cover::Point::spot& s)
{
    auto& w = a.main().world();
    const auto player = w.find_object(threat);
    if (!player)
        return false;
    return !finder.hidden_at(w, player->position(), s, Vector2ui(c.bbox_size));
}

// The script moves its hold to the phase of an order, and options follows, so that apply_options
// doesn't move it back.
bool cover_ai_test::send(critter& c, const Cover::Point::spot& s, bool peek)
{
    const auto ok = peek ? Cover::Script::send_to_peek(c, s) : Cover::Script::send_to_cover(c, s);
    last_note = ok ? note::none : note::no_path;
    if (ok && options.hold != hold_phase::none)
        options.hold = peek ? hold_phase::exposed : hold_phase::hidden;
    return ok;
}

// Ticked, the box holds the phase the npc stands in or walks to. An npc in seek or retreat hides
// at the cover it finds next, so it is held hidden, as with no npc.
u32 cover_ai_test::toggle_hold()
{
    if (options.hold != hold_phase::none)
        options.hold = hold_phase::none;
    else
    {
        const auto c = npc.lock();
        const auto s = c ? Cover::Script::get_status(*c) : Optional<Cover::Script::status>{};
        const auto out = s && (s->st == state::exposed || s->st == state::to_peek);
        options.hold = out ? hold_phase::exposed : hold_phase::hidden;
    }
    apply_options();
    return (u32)options.hold;
}

u32 cover_ai_test::toggle_move_to_end()
{
    options.max_cover_end_dist = options.max_cover_end_dist ? 0 : Cover::Script::options{}.max_cover_end_dist;
    apply_options();
    return options.max_cover_end_dist ? 1 : 0;
}

// Caches the script's last search's spots until the next search, a pass_gen change or another npc.
// Critter movement leaves pass_gen unchanged, so a spot the player walks onto stays listed.
ArrayView<const Cover::Point::spot> cover_ai_test::script_spots(app& a, critter& c, const Cover::Script::status& s)
{
    const auto gen = a.main().world().pass_gen();
    if (!has_script_list || script_list_npc != c.id || script_list_num_searches != s.num_searches ||
        script_list_pass_gen != gen)
    {
        const auto f0 = fills(a);
        auto t = Time::now();
        Cover::Script::list_spots(c, script_list);
        const auto dt = t.update();
        const auto f = fills(a);
        fm_debug("cover-ai: cover list: %u spots, %.1f us, %u octant fills in %.1f us", (u32)script_list.size(),
                 us(dt), f.n - f0.n, us(f.time - f0.time));
        script_list_npc = c.id;
        script_list_num_searches = s.num_searches;
        script_list_pass_gen = gen;
        has_script_list = true;
    }
    return script_list;
}

// Sends the npc to hide at the first of script_spots() after its cover, or before it when back is
// set, wrapping around, that the player doesn't see and a path leads to. When its cover isn't
// listed, the search starts at the list's first spot, or its last when back is set. Returns the
// spot's index in that list, or empty when there is none.
Optional<u32> cover_ai_test::step_cover(app& a, bool back)
{
    const auto c = npc.lock();
    const auto s = c ? Cover::Script::get_status(*c) : Optional<Cover::Script::status>{};
    if (!s)
        return {};
    const auto list = script_spots(a, *c, *s);
    const auto n = (u32)list.size();
    auto from = 0u;
    while (from < n && !(s->has_cover && list[from].pos == s->canonical))
        from++;
    const auto listed = from < n;
    if (!listed)
        from = back ? 0 : n - 1;
    for (auto j = 0u; j < n; j++)
    {
        const auto k = back ? (from + n - 1 - j) % n : (from + 1 + j) % n;
        if (listed && k == from || player_sees(a, *c, list[k]))
            continue;
        if (send(*c, list[k], false))
            return k;
    }
    last_note = note::no_other_cover;
    return {};
}

// A left click picks the nearest dot of the overlay within pick_radius, a listed spot's on a tie.
// A listed spot's dot sends the npc to hide there, and a peek spot's dot sends it to peek from a
// spot it belongs to. Of spots sharing a peek spot, as unmoved spots along one face often do, it
// peeks from its cover if that is among them, else from the nearest.
bool cover_ai_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    if (is_down || e.button != mouse_button_left || !show_spots)
        return false;
    const auto c = npc.lock();
    const auto s = c ? Cover::Script::get_status(*c) : Optional<Cover::Script::status>{};
    if (!s)
        return false;
    auto best = pick_radius*pick_radius;
    auto k = (u32)-1;
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
    if (k == (u32)-1)
        return false;
    if (peek)
    {
        const auto pt = spot_states[k].peek.pos;
        for (auto i = 0u; i < spot_states.size(); i++)
        {
            if (i == k || !spot_states[i].has_peek || spot_states[i].peek.pos != pt)
                continue;
            if (s->has_cover && spots[k].pos == s->canonical)
                break;
            if (s->has_cover && spots[i].pos == s->canonical ||
                point::distance_squared(spots[i].pos, pt) < point::distance_squared(spots[k].pos, pt))
                k = i;
        }
    }
    (void)send(*c, spots[k], peek);
    return true;
}

void cover_ai_test::update_rates(const Cover::Script::status& s)
{
    const counts now{s.num_searches, s.num_rays, s.num_dijkstra_calls};
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
        .num_searches = (float)(now.num_searches - snapshot.num_searches) / secs,
        .num_rays = (float)(now.num_rays - snapshot.num_rays) / secs,
        .num_dijkstra_calls = (float)(now.num_dijkstra_calls - snapshot.num_dijkstra_calls) / secs,
    };
    snapshot = now;
    snapshot_time = clock;
}

// Casts the status's ray from gun_ray_from to gun again, with the script's pool and filter, so the
// drawn line and the "ray" row show a ray the script found clear, at grazing angles too, unless the
// world changed since. The line starts at the player whenever the script checked the gun after the
// player's last move, and otherwise where the player stood at the last check.
void cover_ai_test::update_ray(app& a, const critter& c, const Cover::Script::status& s)
{
    ray.has_result = false;
    if (s.st != state::exposed || !s.has_peek)
        return;
    auto& m = a.main();
    auto& pool = m.astar().pool_registry().pass_pool(Pass::Params{options.div_size, pass_mode::shoot_through});
    ray = raycast(m.world(), s.gun_ray_from, s.gun, c.id, pool, Search::without_critters());
}

void cover_ai_test::update_post(app& a, const Ns& dt)
{
    clock += dt;
    const auto c = npc.lock();
    const auto s = c ? Cover::Script::get_status(*c) : Optional<Cover::Script::status>{};
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
    log_script(a, *c, *s);
    if (!show_spots)
        return;
    if (spots_stale(a))
        restart_spots(a, *c);
    else
        drop_seen_spots(a, *c);
    search_spots(a, *c, true);
}

void cover_ai_test::update_pre(app& a, const Ns&)
{
    fills_before_update = fills(a);
}

// The octant fills are those of the whole world update, of which the script's update is the only
// part that reads cover grids.
void cover_ai_test::log_script(app& a, const critter& c, const Cover::Script::status& s)
{
    const auto& l = logged;
    if (c.id == logged_npc && s.num_searches == l.num_searches &&s.has_cover == l.has_cover && s.cover == l.cover &&
        s.has_peek == l.has_peek && s.peek == l.peek)
        return;
    logged = s;
    logged_npc = c.id;
    const auto f = fills(a);
    char cover[32], peek[32];
    print_spot(cover, s.has_cover, s.cover);
    print_spot(peek, s.has_peek, s.peek);
    fm_debug("cover-ai: search %u, %s, cover %s, peek %s: script update %.1f us, %u octant fills in %.1f us",
             s.num_searches, state_names[(usize)s.st], cover, peek, us(s.last_update), f.n - fills_before_update.n,
             us(f.time - fills_before_update.time));
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
            search_spots(a, *c, false);
        }
        ret.type = ValueType::u32;
        ret.u32 = (u32)spots.size();
        break;
    case 4:
    case 5:
        if (const auto k = step_cover(a, v.u32 == 5))
        {
            ret.type = ValueType::u32;
            ret.u32 = *k;
        }
        break;
    case 6:
    case 7:
        ret.type = ValueType::u32;
        ret.u32 = v.u32 == 6 ? toggle_hold() : toggle_move_to_end();
        break;
    case 8:
        set_div(a, v.au32[1]);
        ret.type = ValueType::u32;
        ret.u32 = options.div_size;
        break;
    case 9:
        ret.type = ValueType::au32;
        ret.au32[0] = has_search ? next_tile : 0;
        ret.au32[1] = has_search ? (u32)tiles.size() : 0;
        ret.au32[2] = search_fills.n;
        ret.au32[3] = has_search;
        break;
    case 10:
        ret.type = ValueType::au32;
        ret.au32[0] = 0;
        if (const auto c = npc.lock())
            ret.au32[0] = seen_spots(a, *c);
        ret.au32[1] = dropped_spots;
        ret.au32[2] = (u32)spots.size();
        break;
    case 11:
        ret.type = ValueType::ai32;
        ret.ai32[0] = Vector3i(search_center).x();
        ret.ai32[1] = Vector3i(search_center).y();
        ret.ai32[2] = (i32)search_octant;
        ret.ai32[3] = has_search;
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
    const auto s = Cover::Script::get_status(*c);
    if (!s)
        return;
    ImDrawList& fg = *ImGui::GetForegroundDrawList();
    if (s->has_cover)
        draw_dot(fg, a, s->cover, 6, {0.2f, 1, 0.2f, 1});
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
    auto s = c ? Cover::Script::get_status(*c) : Optional<Cover::Script::status>{};
    auto d = begin_disabled(!s);

    if (ImGui::ArrowButton("##prev-cover", ImGuiDir_Left))
    {
        (void)step_cover(a, true);
        s = Cover::Script::get_status(*c);
    }
    ImGui::SameLine();

    ArrayView<const Cover::Point::spot> list;
    if (s)
        list = script_spots(a, *c, *s);
    auto cur = (u32)-1;
    for (auto i = 0u; i < list.size(); i++)
        if (s->has_cover && list[i].pos == s->canonical)
        {
            cur = i;
            break;
        }
    if (list.isEmpty())
        snformat(preview, "no spots listed");
    else if (cur == (u32)-1)
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
                (void)send(*c, list[i], false);
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

    bool hold = options.hold != hold_phase::none;
    if (ImGui::Checkbox("Hold", &hold))
        (void)toggle_hold();
    bool to_end = options.max_cover_end_dist != 0;
    if (ImGui::Checkbox("Move spots to cover's end", &to_end))
        (void)toggle_move_to_end();
    bool reach = options.threat_reach != 0;
    if (ImGui::Checkbox("Hide from where the player can walk in 2 tiles", &reach))
    {
        options.threat_reach = reach ? Cover::Script::options{}.threat_reach : 0;
        apply_options();
    }
    if (ImGui::Checkbox("Only cover with a peek spot", &options.needs_peek))
        apply_options();
    using fallback = Cover::Script::options::fallback;
    text("If no cover passes, take one");
    auto last = (int)options.last_resort;
    auto changed = ImGui::RadioButton("never", &last, (int)fallback::none);
    ImGui::SameLine();
    changed |= ImGui::RadioButton("without a peek spot", &last, (int)fallback::no_peek);
    ImGui::SameLine();
    changed |= ImGui::RadioButton("or seen in 2 tiles", &last, (int)fallback::in_reach);
    if (changed)
    {
        options.last_resort = (fallback)last;
        apply_options();
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

    snformat(buf, "{}x{}", options.div_size, options.div_size);
    if (auto b = begin_combo("Cover div", buf))
        for (const auto n : div_choices)
        {
            snformat(buf2, "{}x{}", n, n);
            if (ImGui::Selectable(buf2, n == options.div_size))
                set_div(a, n);
        }

    if (ImGui::Checkbox("Other cover spots", &show_spots) && !show_spots)
        clear_spots();
    if (show_spots)
    {
        auto e = std::countr_zero((unsigned)radius_tiles);
        snformat(buf, "{} tiles, {} px", radius_tiles, radius_tiles*tile_size_xy);
        if (ImGui::SliderInt("Spots radius", &e, 0, max_spots_radius_log2, buf, slider_flags))
            radius_tiles = 1 << e;
        ImGui::TextWrapped("Spots hidden from the player, searched a few tiles per frame with at most one octant fill, "
                           "starting over when the player's bearing from the npc's tile enters another of the "
                           "cover grid's 32 octants, or a wall changes. Other player moves drop the spots the "
                           "player then sees. Click a blue dot to send "
                           "the npc to hide there, or an orange one to peek from its spot. With Hold "
                           "ticked, it stays there.");
    }

    const auto c = npc.lock();
    const auto s = c ? Cover::Script::get_status(*c) : Optional<Cover::Script::status>{};
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
        text(!c ? "no npc" : !s ? "no cover script" : state_names[(usize)s->st]);

        do_column("progress");
        if (s && (s->st == state::search || s->st == state::retreat && s->search_tiles_left))
            snformat(buf, "tile {} of {}", s->search_tile, s->num_search_tiles);
        else if (s && s->st == state::find_retreat)
            snformat(buf, "spot {} of {}, pass {}, {} without a path", s->retreat_spot, s->num_retreat_spots,
                     s->retreat_all ? 2 : 1, s->num_failed_retreat_dijkstra_calls);
        else
            snformat(buf, "-");
        text(buf);

        do_column("npc");
        if (c)
        {
            print_coord(buf2, c->position());
            snformat(buf, "{} {}", (u64)c->id, (const char*)buf2);
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

        do_column("listed spots");
        if (show_spots && has_search)
        {
            auto peeks = 0u;
            for (const auto& x : spot_states)
                peeks += x.has_peek;
            snformat(buf, "{}, {} with peek spots, {} dropped as seen", spots.size(), peeks, dropped_spots);
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
        rate_row("searches/s", rates.num_searches);
        rate_row("rays/s", rates.num_rays);
        rate_row("Dijkstra searches/s", rates.num_dijkstra_calls);
    }
}

} // namespace

Pointer<base_test> tests_data::make_test_cover_ai() { return Pointer<cover_ai_test>{InPlaceInit}; }

} // namespace floormat::tests
