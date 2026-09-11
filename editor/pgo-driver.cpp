#include "pgo-driver.hpp"
#include "pgo-scenes.hpp"
#include "app.hpp"
#include "editor.hpp"
#include "ground-editor.hpp"
#include "wall-editor.hpp"
#include "keys.hpp"
#include "tests-private.hpp"
#include "loader/loader.hpp"
#include "loader/ground-cell.hpp"
#include "loader/wall-cell.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/point.inl"
#include "src/tile-defs.hpp"
#include "src/grid.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"
#include "src/tile.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/critter-script.hpp"
#include "src/search-astar.hpp"
#include "src/search-pred.hpp"
#include "src/search-result.hpp"
#include "src/raycast.hpp"
#include "raycast-draw.hpp"
#include "shaders/lightmap.hpp"
#include <mg/Image.h>
#include <mg/PixelFormat.h>
#include "src/nanosecond.hpp"
#include "compat/assert.hpp"
#include "compat/enum-bitset.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/split-string.hpp"
#include "imgui-raii.hpp"
#include "floormat/events.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include <cstdio>
#include <mg/Functions.h>
#include <cr/GrowableArray.h>

namespace floormat {

ArrayView<const pgo::scene> app::scenes() noexcept
{
    constexpr auto SV_flags = StringViewFlag::Global|StringViewFlag::NullTerminated;
#define FM_SCENE(name, mode_) { ( StringView{#name, array_size(#name)-1, SV_flags} ), ( &app::name ), driver_mode::mode_, }
    static constexpr pgo::scene Scenes[] = {
        FM_SCENE(scene_modes, coverage),
        FM_SCENE(scene_ground_editor, coverage),
        FM_SCENE(scene_drag_paint, coverage),
        FM_SCENE(scene_benchmark, profile),
        FM_SCENE(scene_walk, profile),
        FM_SCENE(scene_maze, profile),
        FM_SCENE(scene_maze2, profile),
        FM_SCENE(scene_raycast, profile),
        FM_SCENE(scene_object_ids, profile),
        FM_SCENE(scene_grids, profile),
        FM_SCENE(scene_lightmap, profile),
        FM_SCENE(scene_cover, profile),
    };
#undef FM_SCENE

    static_assert(array_size(Scenes) <= 32); // pgo::state::scene_mask

    return Scenes;
}

} // namespace floormat

namespace floormat::pgo {

task task::promise_type::get_return_object() { return task{handle_type::from_promise(*this)}; }
std::suspend_always task::promise_type::initial_suspend() noexcept { return {}; }
std::suspend_always task::promise_type::final_suspend() noexcept { return {}; }
void task::promise_type::return_void() noexcept {}
void task::promise_type::unhandled_exception() { fm_abort("%s", "exception escaped a driver scene"); }

std::suspend_always task::promise_type::yield_value(pause p) noexcept
{
    fm_assert(p.num_frames > 0);
    current = p;
    // Suspending already costs the rest of this tick, so the stored count is one less
    // than asked. `co_yield {1}` means "resume on the next tick", not "skip a tick".
    frames_to_wait = p.num_frames - 1;
    return {};
}

bool task::promise_type::sub_awaiter::await_ready() const noexcept { return p.sub.done(); }
void task::promise_type::sub_awaiter::await_suspend(handle_type) noexcept {}

task::~task() noexcept { if (h) h.destroy(); }
task::task(task&& other) noexcept: h{other.h} { other.h = {}; }

task& task::operator=(task&& other) noexcept
{
    if (this != &other)
    {
        if (h)
            h.destroy();
        h = other.h;
        other.h = {};
    }
    return *this;
}

void task::start()
{
    // initial_suspend is suspend_always, so the body hasn't started yet.
    if (h && !h.done())
        h.resume();
}

void task::tick()
{
    if (done())
        return;
    auto& p = h.promise();
    if (!p.sub.done())
    {
        p.sub.tick();
        if (!p.sub.done())
            return;
        // Falls through on the tick the sub finishes, so this task resumes in that same tick.
        // That is what the loop form's last ++it did.
    }
    if (p.frames_to_wait != 0)
    {
        p.frames_to_wait--;
        return;
    }
    h.resume();
}

task::iterator task::begin() { start(); return iterator{h}; }
task::sentinel task::end() noexcept { return {}; }

task::promise_type::sub_awaiter task::promise_type::await_transform(task&& t)
{
    fm_assert(sub.done());
    sub = move(t);
    sub.start();
    return sub_awaiter{*this};
}

task::iterator& task::iterator::operator++()
{
    // Resuming h directly would skip an active sub-task, so the two forms don't mix.
    fm_assert(h.promise().sub.done());
    h.resume();
    return *this;
}

const pause& task::iterator::operator*() const noexcept { return h.promise().current; }

} // namespace floormat::pgo

namespace floormat {

using pgo::task;

namespace {

// How fast the camera walks a found route, in pixels of world per frame. A critter covers about
// 9 px a frame, so this is a brisk walk -- roughly a chunk and a half of screen a second at 60 Hz.
// The fixed frame count this replaces divided the whole route by 60 instead, which on the longest
// maze leg came to a chunk of world per frame.
constexpr float pan_speed = tile_size_xy/4.f;
// Walking all of a maze route at that rate would take minutes -- one leg is 65605 px. So it is
// cut into segments and each gets a frame budget, which puts the camera in four places along the
// route instead of crawling the first chunk of it. The overlay draws the whole route regardless.
constexpr uint32_t pan_segments = 4, pan_segment_frames = 64;
constexpr float pan_segment_px = (float)pan_segment_frames * pan_speed;

// Which waypoints of scene_raycast's circle one frame's batch of rays takes. `radar` takes the
// batch adjacent and advances the whole wedge by its own size, so the beam sweeps exactly its own
// width a frame -- 1.2 to 5.9 degrees depending on radius, a revolution in one to five seconds --
// and covers the circle with no gap and no overlap. `spokes` instead cuts the circle into one
// sector per ray and takes the same offset inside each, which spreads the batch out but stops
// reading as motion; there the per-frame advance is a stride that has to be coprime with the frame
// count. Either casts every waypoint exactly once per radius.
enum class ray_order : uint8_t { spokes, radar };
constexpr inline auto sweep_order = ray_order::radar;

constexpr uint32_t gcd_(uint32_t a, uint32_t b)
{
    while (b != 0)
    {
        const auto t = a % b;
        a = b;
        b = t;
    }
    return a;
}

// Which offset inside a sector frame f takes, under `spokes`. Taking them in order advances every
// spoke by one waypoint a frame -- a fiftieth of a degree at the longest radius -- so consecutive
// frames draw the same picture. An eighth of a sector is 1.4 degrees a frame, a revolution in
// about four seconds, and still far enough that no frame repeats its neighbour's rays. Any stride
// coprime with the frame count visits every offset exactly once, which is what keeps the sweep
// exhaustive; the golden ratio does that too but spins the wheel faster than the eye follows.
constexpr inline uint32_t sweep_sector_div = 8;

uint32_t frame_stride(uint32_t num_frames)
{
    auto s = num_frames / sweep_sector_div;
    while (s > 1 && gcd_(s, num_frames) != 1)
        s--;
    return Math::max(s, 1u);
}

} // namespace

void app::set_key_state(key k, bool is_pressed)
{
    // One-shot keys are wiped by clear_non_repeated_keys() every frame, so holding one does
    // nothing. Use do_key() for those.
    fm_assert(k < key_NO_REPEAT);
    (*keys_)[k] = is_pressed;
}

void app::set_modifier_state(int mod, bool is_pressed)
{
    fm_assert((mod & ~kmod_mask) == 0);
    auto& mods = _driver->mods;
    mods = is_pressed ? mods | mod : mods & ~mod;
}

void app::set_mouse_button_state(uint8_t button, bool is_pressed)
{
    fm_assert(button != mouse_button_none);
    // do_mouse_up_down() compares for equality, so exactly one bit at a time.
    fm_assert((button & (button - 1)) == 0);
    auto& held = _driver->held_buttons;
    held = mouse_button(is_pressed ? held | button : held & ~button);
    // Same order as on_mouse_up_down(). do_mouse_up_down() does nothing at all in tests mode,
    // so the dispatcher is the only route a driver click has into a test.
    if (_editor->mode() == editor_mode::tests)
    {
        const mouse_button_event e = {
            .position = Vector2(cursor.pixel ? *cursor.pixel : Vector2i{}),
            .mods = _driver->mods,
            .button = mouse_button(button),
            .click_count = 1,
        };
        if (tests_handle_mouse_click(e, is_pressed))
            return;
    }
    do_mouse_up_down(button, is_pressed, _driver->mods);
}

void app::move_cursor_to(point pt)
{
    center_camera_on(pt);
    // center_camera_on() updates cursor.tile, but do_mouse_move() is the only route to
    // editor::on_mouse_move(), which is what fills tiles while a button is held.
    do_mouse_move(_driver->mods);
}

void app::scroll(int8_t offset)
{
    do_mouse_scroll(offset);
}

// `path` is borrowed for the whole pan. A coroutine frame copies the view and not the array
// behind it, so whatever owns the path has to outlive the co_await.
task app::pan_along_path(ArrayView<const point> path)
{
    fm_assert(!path.isEmpty());
    const auto n = (uint32_t)path.size();
    // A copy, unlike this function's own borrowed view. pgo::state is not scoped to the
    // coroutine, so a view left there dangles if the frame is ever abandoned mid-pan.
    _driver->route = Array<point>{InPlaceInit, path};

    // Where the last segment starts, so it ends on the goal rather than three quarters of the way
    // along. Rewinding to a whole waypoint can stop a leg short of it, and a leg averages 500 px,
    // so rewind by distance and land inside one.
    auto last_seg = n - 1;
    auto last_start = path[n - 1];
    for (float back = pan_segment_px; last_seg > 0; )
    {
        const auto d = path[last_seg] - path[last_seg-1];
        const auto len = Vector2(d).length();
        last_seg--;
        last_start = path[last_seg];
        if (len >= back)
        {
            last_start = point::normalize_coords(last_start, Vector2i(Vector2(d) * (1 - back/len)));
            break;
        }
        back -= len;
    }

    for (auto seg = 0u; seg < pan_segments; seg++)
    {
        const bool is_last = seg + 1 == pan_segments;
        auto i = is_last ? last_seg : seg * n / pan_segments;
        auto cur = is_last ? last_start : path[i];
        auto frames = pan_segment_frames;

        // The entries are waypoints and not steps -- maze2's 20705 px route is 35 of them, 590 px
        // apart -- so one a frame would still teleport the camera. Walk each leg of the polyline.
        while (frames > 0 && i + 1 < n)
        {
            const auto d = path[i+1] - cur;
            const auto steps = Math::max(1u, (uint32_t)(Vector2(d).length()/pan_speed));
            for (auto k = 1u; k <= steps && frames > 0; k++, frames--)
            {
                center_camera_on(point::normalize_coords(cur, d * (int)k / (int)steps));
                co_yield {};
            }
            i++;
            cur = path[i];
        }
    }

    _driver->route = {};
}

// The press and the release straddle a frame, so something renders with the button held and a
// handler that acts on one edge gets a frame to do it in before the other arrives.
task app::click_at_cursor(uint8_t button)
{
    set_mouse_button_state(button, true);
    co_yield {};
    set_mouse_button_state(button, false);
}

task app::click_at(point pt, uint8_t button)
{
    move_cursor_to(pt);
    co_await click_at_cursor(button);
}

void app::release_all_input()
{
    for (uint32_t b = 1; b <= mouse_button_x2; b <<= 1)
        if (_driver->held_buttons & b)
            set_mouse_button_state((uint8_t)b, false);
    clear_keys();
    _driver->mods = 0;
    // A press that placed something returns early without reaching on_release(), so the drag
    // state can outlive every button being up.
    _editor->on_release();
}

point app::cursor_point()
{
    auto pt = cursor_state().point();
    fm_assert(pt);
    return *pt;
}

task app::press_and_hold(point pt, uint8_t button, uint32_t num_frames)
{
    move_cursor_to(pt);
    set_mouse_button_state(button, true);
    co_yield {num_frames};
    set_mouse_button_state(button, false);
}

task app::drag_along(point from, Vector2i step, uint32_t count, uint8_t button)
{
    fm_assert(count > 0);
    auto pt = from;
    move_cursor_to(pt);
    set_mouse_button_state(button, true);
    co_yield {};
    for (auto i = 1u; i < count; i++)
    {
        pt = pt + step*tile_size_xy;
        move_cursor_to(pt);
        co_yield {};
    }
    set_mouse_button_state(button, false);
}

task app::scene_modes()
{
    static constexpr key modes[] = {
        key_mode_none, key_mode_floor, key_mode_walls,
        key_mode_scenery, key_mode_vobj, key_mode_tests,
    };
    for (auto k : modes)
    {
        do_key(k);
        co_yield {};
    }

    // do_mouse_up_down() short-circuits every other mode while tests mode is active, so later
    // steps would silently place nothing.
    do_key(key_mode_none);
    fm_assert(_editor->mode() == editor_mode::none);
    co_yield {};

    static constexpr key toggles[] = {
        key_render_collision_boxes, key_render_clickables, key_render_vobjs,
    };
    for (auto k : toggles)
    {
        do_key(k);
        co_yield {};
        do_key(k); // back to the startup state
        co_yield {};
    }

    // Modifiers are held state, and get_key_modifiers() has to report the driver's rather than
    // the physical keyboard's -- do_camera() feeds it into do_mouse_move() on every pan.
    fm_assert_equal(0, get_key_modifiers());
    set_modifier_state(kmod_ctrl, true);
    fm_assert_equal((int)kmod_ctrl, get_key_modifiers());
    set_modifier_state(kmod_shift, true);
    fm_assert_equal((int)(kmod_ctrl|kmod_shift), get_key_modifiers());
    co_yield {};
    set_modifier_state(kmod_ctrl, false);
    set_modifier_state(kmod_shift, false);
    fm_assert_equal(0, get_key_modifiers());
    co_yield {};
}

task app::scene_ground_editor()
{
    do_set_mode(editor_mode::floor);
    auto* ed = _editor->current_ground_editor();
    fm_assert(ed);

    uint32_t n = 0;

    for (const auto& [name, cell] : *ed)
    {
        if (!cell.atlas)
            continue;

        const auto variants = (uint32_t)cell.atlas->num_tiles();
        for (auto v = 0u; v < variants; v++)
        {
            ed->select_tile(cell.atlas, v);
            fm_assert(ed->is_anything_selected());
            fm_assert(ed->is_atlas_selected(cell.atlas));
            fm_assert(ed->is_tile_selected(cell.atlas, v));

            const point pt{{{0, 0, 0}, {(uint8_t)(n % TILE_MAX_DIM), (uint8_t)(n / TILE_MAX_DIM % TILE_MAX_DIM)}}, {}};
            move_cursor_to(pt);
            fm_assert_equal(pt, cursor_point());
            // click_at() suspends between press and release, so there is no separate pause.
            co_await click_at(pt, mouse_button_left);
            n++;
        }

        ed->select_tile_permutation(cell.atlas);
        fm_assert(ed->is_permutation_selected(cell.atlas));
        // get_selected_perm() pops one variant and only reshuffles once the array runs dry, so
        // it takes more than num_tiles() calls to reach the fisher_yates branch a second time.
        for (auto i = 0u; i < variants + 2; i++)
            fm_assert(ed->get_selected());
        co_yield {};
    }

    ed->clear_selection();
    fm_assert(!ed->is_anything_selected());
    co_yield {};
}

// Proves three things at once: a button survives across co_yield, a sub-task's yields pass
// through the caller with the frame count intact, and the drag actually painted the world.
task app::scene_drag_paint()
{
    do_set_mode(editor_mode::walls);
    auto* ed = _editor->current_wall_editor();
    fm_assert(ed);

    bptr<wall_atlas> atlas;
    for (const auto& [name, cell] : *ed)
        if (cell.atlas)
        {
            atlas = cell.atlas;
            break;
        }
    fm_assert(atlas);
    ed->select_atlas(atlas);
    fm_assert(ed->is_atlas_selected(atlas));

    // check_snap() ignores mods for walls and derives the axis from the rotation, so a north
    // wall must be dragged east and a west wall south for the run to stay on one line.
    static constexpr struct { enum rotation r; Vector2i step; } runs[] = {
        { rotation::N, {1, 0} },
        { rotation::W, {0, 1} },
    };

    constexpr uint32_t count = 8;
    auto& w = M->world();

    for (const auto& [r, step] : runs)
    {
        ed->set_rotation(r);
        fm_assert(ed->rotation() == r);

        const point from{{{0, 0, 0}, {2, 2}}, {}};
        const auto frames_before = _driver->frames_run;
        for (const auto& p : drag_along(from, step, count, mouse_button_left))
        {
            // A held button must survive the suspension; that is what makes a drag a drag.
            fm_assert(_driver->held_buttons == mouse_button_left);
            co_yield p;
        }
        // One frame per point, so the sub-task's pauses reached the runner unscaled.
        fm_assert_equal(count, _driver->frames_run - frames_before);
        fm_assert(_driver->held_buttons == mouse_button_none);

        for (auto i = 0u; i < count; i++)
        {
            const auto pt = from + step*(int)i*tile_size_xy;
            // at(), not operator[]: an assertion that lazily creates the chunk it's checking
            // would pass even if the drag never reached this tile.
            auto* c = w.at(pt.chunk3());
            fm_assert(c);
            auto t = (*c)[pt.local()];
            const auto got = r == rotation::N ? t.wall_north_atlas() : t.wall_west_atlas();
            fm_assert(got == atlas);
        }
        co_yield {};
    }

    ed->clear_selection();
    fm_assert(!ed->is_anything_selected());
    do_set_mode(editor_mode::none);
    co_yield {};

    // The drag above only ever yields {1}, so it never showed that a multi-frame pause
    // survives delegation. A sub-task's count must reach the runner unscaled: not collapsed
    // to one frame, and not multiplied by the outer loop.
    {
        constexpr uint32_t held = 30;
        const point pt{{{0, 0, 0}, {4, 4}}, {}};
        const auto before = _driver->frames_run;
        co_await press_and_hold(pt, mouse_button_left, held);
        fm_assert_equal(held, _driver->frames_run - before);
        fm_assert(_driver->held_buttons == mouse_button_none);
    }
    co_yield {};
}

task app::scene_benchmark()
{
    populate_scene_benchmark();
    co_yield {};

    static constexpr key pans[] = {
        key_camera_right, key_camera_down, key_camera_left, key_camera_up,
    };
    for (auto k : pans)
    {
        set_key_state(k, true);
        co_yield {60};
        set_key_state(k, false);
        co_yield {};
    }
}

task app::scene_walk()
{
    // The generator puts the player on the corridor's north end, on the first passable column.
    // Baffles alternate sides down the corridor, so the route weaves between the columns either
    // side of them rather than running straight; the goal column is clear of the last one.
    constexpr uint8_t col = pgo::walk_corridor_tile + 1;
    // The corridor is the same length and the search is the same search; this only decides how
    // many frames the critter spends covering it.
    constexpr float speed_mult = 3;
    populate_scene_benchmark_walkable(pgo::walk_corridor_width);
    auto& w = M->world();
    auto C = ensure_player_character(w);
    co_yield {};

    const auto from = C->position();
    // update_world() only updates objects inside get_draw_bounds(), so an off-screen critter
    // never has its script ticked at all.
    center_camera_on(from);
    const auto to = point{chunk_coords_{0, pgo::walk_chunk_max, 0},
                          local_coords{col, TILE_MAX_DIM-3}, {}};
    const auto dist = point::distance(from, to)*2 + tile_size_xy * TILE_MAX_DIM;
    auto res = M->astar().Dijkstra(w, from, to, dist, Vector2ui{C->bbox_size},
                                   Search::without_critters());
    if (res.empty())
    {
        ERR_nospace << "driver: no path " << from << " -> " << to;
        if (driver_stop("walk: no path"_s))
        {
            // Handed back, so leave the maze, the critter and the camera as they are and open
            // Path search on them -- a click re-runs the query that just failed.
            do_key(key_mode_tests);
            tests().switch_to(floormat::tests::Test::path);
        }
        co_return;
    }
    // res is moved into the script below, so the copy the overlay draws has to be taken first.
    auto& D = *_driver;
    D.route = Array<point>{InPlaceInit, res.path()};

    C->speed *= speed_mult;
    C->script.do_reassign(critter_script::make_walk_script(move(res)), move(C));

    const auto n = (uint32_t)D.route.size();
    float route_len = 0;
    for (auto i = 1u; i < n; i++)
        route_len += point::distance(D.route[i-1], D.route[i]);

    // The script moves the critter from update() and reports no completion, so watch for the
    // position going quiet instead.
    auto last = from;
    for (auto i = 0u; i < 400; i++)
    {
        co_yield {15};
        auto Cʹ = w.find_object<critter>(_character_id);
        if (!Cʹ)
            break;
        const auto pos = Cʹ->position();
        if (pos == last)
            break;
        last = pos;
        center_camera_on(pos);
    }
    D.route = {};

    // The poll loop exits the first time the position stops changing, so a critter wedged against
    // a corridor wall ends it just as quietly as one that arrived. Raising speed makes each step
    // longer, which is exactly what would cause that.
    const auto walked = point::distance(from, last);
    const auto straight = point::distance(from, to);
    fm_assert(walked > straight/2);
    // route against straight is what says the baffles are being walked around, not through.
    fm_debug("walk: %ux speed, %u waypoints, route %.0f px over %.0f straight, ended %.0f px out",
             (uint32_t)speed_mult, n, (double)route_len, (double)straight, (double)walked);
}

// The maze's own check, kept apart from the ray sweep because it never had anything to do with
// it. Dijkstra<1> prints len/len0/ratio: a ratio near 1 would mean the carve left a straight shot
// open and the dead ends cost A* nothing.
task app::scene_maze()
{
    // A tour of the four corner cells rather than one diagonal. Each leg is a search in its own
    // right, and the two that run along an edge have the shortest straight line against the same
    // snake corridor, so octile_distance tells A* the least about those.
    constexpr uint32_t num_legs = 4;

    populate_scene_maze();
    auto& w = M->world();
    auto C = ensure_player_character(w);
    center_camera_on(C->position());
    co_yield {};

    for (auto leg = 0u; leg < num_legs; leg++)
    {
        const auto from = maze_corner(leg), to = maze_corner(leg + 1);
        // Every cell walked once bounds the true path; the ratio never gets near that.
        const auto max_dist = point::distance(from, to) * 32;
        auto res = M->astar().Dijkstra<1>(w, from, to, max_dist, Vector2ui{C->bbox_size},
                                          Search::without_critters());
        if (res.empty())
        {
            ERR_nospace << "driver: maze has no path " << from << " -> " << to;
            if (driver_stop("maze: unroutable"_s))
            {
                // Handed back, so leave the maze and the camera as they are and open Path search
                // on them -- a click re-runs the query that just failed.
                do_key(key_mode_tests);
                tests().switch_to(floormat::tests::Test::path);
            }
            co_return;
        }

        // The search is the whole scene, so without this it renders two frames several seconds
        // apart and the window reads as hung. Panning the route also runs the draw path across
        // the world that was just searched, which one centred frame never does.
        co_await pan_along_path(res.path());
    }
}


task app::scene_raycast()
{
    // A ray over a field of small pins (see generate_raycast_pins). What this measures is a
    // collider the DDA's cell rectangle catches and ray_aabb_intersection then rejects, so the ray
    // has to pass near things without meeting them head-on. pin_pitch is sized against the longest
    // radius below; shortening that without the pitch stops every ray in the first tile.
    //
    // Several radii rather than one. Collisions along a ray go as its length, so a quarter-chunk
    // ray mostly gets through while a full-chunk one mostly stops. Those are different paths
    // through the DDA and one radius only ever shows one of them. They live in pgo-scenes.hpp
    // because the pin field is sized against the longest of them from another file.
    constexpr uint32_t num_sampled = 64;
    // Driving the raycast test instead cost a frame per ray, and the profile came out mostly
    // swap and ImGui. The scene runs raycast_with_diag() itself, so the batch size is its own
    // choice rather than the test's queue depth, and driver_draw_overlay() draws all of them.
    constexpr uint32_t rays_per_frame = pgo::max_rays_per_frame;

    populate_scene_raycast_pins();
    auto& w = M->world();
    auto C = ensure_player_character(w);
    const auto from = C->position();
    center_camera_on(from);

    // The distinct integer endpoints on the circle, in angular order. Sampling 8*r evenly spaced
    // angles -- one per pixel of circumference -- both repeats and misses: a fifth of the samples
    // land on the previous sample's pixel and cast an identical ray, while a sixth of the pixels
    // the circle passes through are never sampled at all. Stepping the angle finer and keeping
    // only the changes fixes both, and gives an exact count to batch frames against. 0.15 px
    // finds 99.8% of what 0.125 px does for a sixth of the trig.
    constexpr float waypoint_step_px = .15f;
    const auto build_waypoints = [](int ray_len) {
        const auto n = (uint32_t)((float)(8*ray_len) / waypoint_step_px);
        const auto step = 2.f * Math::Constants<float>::pi() / (float)n;
        Array<Vector2i> out;
        arrayReserve(out, 8*(uint32_t)ray_len);
        for (auto i = 0u; i < n; i++)
        {
            const auto theta = Rad{(float)i * step};
            const auto pt = Vector2i(Vector2{Math::cos(theta), Math::sin(theta)} * (float)ray_len);
            if (out.isEmpty() || pt != out.back())
                arrayAppend(out, pt);
        }
        // The walk closes on the pixel it started from.
        if (out.size() > 1 && out.back() == out.front())
            arrayRemoveSuffix(out, 1);
        return out;
    };

    uint32_t num_pins = 0;
    for (auto& c : w.chunks())
        num_pins += (uint32_t)c.objects().size();
    num_pins--; // the player

    // A cursor is allowed past the window edge -- the bounds test lives in on_mouse_up_down(),
    // which the driver bypasses. The sweep leaves the viewport on its own at this radius; this
    // states the property in one place instead of leaving it incidental.
    const auto win = M->window_size();
    const auto far_away = point::normalize_coords(from, Vector2i{4*(int)chunk_size_xy});
    set_cursor_at(far_away);
    (void)cursor_point();
    {
        const auto px = Vector2i(point_screen_pos(far_away));
        fm_assert(px.x() < 0 || px.y() < 0 || px.x() >= win.x() || px.y() >= win.y());
    }

    auto& D = *_driver;
    // Waypoint zero of every radius, since cos 0 is 1 and sin 0 is 0.
    set_cursor_at(point::normalize_coords(from, Vector2i{pgo::raycast_radii[0], 0}));
    co_yield {};

    uint32_t num_rays = 0, total_frames = 0;
    const auto frame0 = w.frame_no();

    for (const auto ray_len : pgo::raycast_radii)
    {
        const auto wp = build_waypoints(ray_len);
        const auto n = (uint32_t)wp.size();
        // raycast_test keeps its result in an anonymous namespace, so a sample run here is the
        // only way to know the field obstructs at all. Both extremes are wrong: too few hits means
        // the pins are too sparse to be in the way, too many that they are a wall and no ray gets
        // walked.
        uint32_t sampled_hits = 0;
        float mean_dist = 0;
        for (auto i = 0u; i < num_sampled; i++)
        {
            const auto to = point::normalize_coords(from, wp[i * n/num_sampled]);
            const auto r = raycast(w, from, to, C->id);
            sampled_hits += !r.success;
            mean_dist += point::distance(from, r.success ? to : r.collision);
        }
        mean_dist /= (float)num_sampled;
        fm_assert(sampled_hits >= num_sampled/8);
        // A hit count alone cannot tell a field that is in the way from one that is a wall, and it
        // has to be retuned whenever the pin size moves. How far a ray gets says both directly.
        fm_assert(mean_dist >= tile_size_xy*2 && mean_dist <= (float)ray_len);

        // One sector per ray of the batch, so the spokes stay 2*pi/32 apart whatever the radius.
        // The waypoint count no longer divides, so the frames whose offset runs off the end of
        // the last sector are one ray short.
        const auto num_frames = (n + rays_per_frame - 1) / rays_per_frame;
        const auto stride = frame_stride(num_frames);
        const auto rays0 = num_rays;

        for (auto f = 0u; f < num_frames; f++)
        {
            const auto off = f*stride % num_frames;
            uint32_t k = 0;
            for (auto j = 0u; j < rays_per_frame; j++)
            {
                const auto idx = sweep_order == ray_order::spokes ? j*num_frames + off
                                                                  : f*rays_per_frame + j;
                if (idx >= n)
                    continue;
                const auto to = point::normalize_coords(from, wp[idx]);
                // Nothing reads the cursor here any more, but the sweep is what the editor's own
                // pixel_to_point() path gets exercised by, and it keeps the pointer on the ray.
                set_cursor_at(to);
                // Asserts the cursor resolved to a world point.
                (void)cursor_point();
                num_rays++;
                D.rays[k] = raycast_with_diag(D.diags[k], w, from, to, C->id);
                k++;
            }
            D.num_rays = k;
            co_yield {};
        }
        total_frames += num_frames;
        // Every waypoint cast exactly once, whichever order they went out in.
        fm_assert_equal(n, num_rays - rays0);
        const auto advance = sweep_order == ray_order::spokes ? stride : rays_per_frame;
        fm_debug("raycast: %u rays around a %d px circle in %u frames, %.2f deg/frame, "
                 "%u/%u sampled rays hit at %.0f px mean",
                 n, ray_len, num_frames, 360.*advance/n, sampled_hits, num_sampled,
                 (double)mean_dist);
    }

    D.num_rays = 0;
    fm_assert_equal((uint64_t)total_frames, w.frame_no() - frame0);

    fm_debug("raycast: %u rays over %u radii in %s batches of %u, %u pins",
             num_rays, (uint32_t)array_size(pgo::raycast_radii),
             sweep_order == ray_order::spokes ? "spoke" : "radar", rays_per_frame, num_pins);
    co_yield {};
}


task app::scene_maze2()
{
    populate_scene_maze2();
    auto& w = M->world();
    auto C = ensure_player_character(w);
    const auto from = C->position();
    center_camera_on(from);
    co_yield {};

    const auto to = maze2_goal();
    const auto max_dist = point::distance(from, to) * 32;
    auto res = M->astar().Dijkstra<1>(w, from, to, max_dist, Vector2ui{C->bbox_size},
                                      Search::without_critters());
    if (res.empty())
    {
        ERR_nospace << "driver: maze2 has no path " << from << " -> " << to;
        if (driver_stop("maze2: unroutable"_s))
        {
            do_key(key_mode_tests);
            tests().switch_to(floormat::tests::Test::path);
        }
        co_return;
    }

    // The search is the whole scene, so without this it renders two frames several seconds apart
    // and the window reads as hung. Panning the route also runs the draw path across the world
    // that was just searched, which one centred frame never does.
    co_await pan_along_path(res.path());
}

// Nothing else in the driver runs the lightmap. do_lightmap_test() keys off tested_light_chunk
// alone and draws every frame it is set, so the scene names a chunk rather than walking the popup
// menu -- a fabricated click cannot reach an ImGui MenuItem.
//
// Cost per frame is two full-image passes per light in the block: a 1024^2 shadow mask over every
// occluder segment of all 16 chunks, then the light quad. num_tested is the frame dial; the
// segment count is generate_lightmap_scene()'s.
task app::scene_cover()
{
    // One rotation a second, wall clock. --driver runs with vsync off, so a frame count would be a
    // different dwell in every build, and this scene exists to be watched. 32 octants, so the whole
    // thing is over half a minute whatever the build -- octant_seconds is the dial.
    constexpr float octant_seconds = 0.3334f;

    populate_scene_cover();
    auto& w = M->world();
    auto C = ensure_player_character(w);
    const auto centre = C->position();
    center_camera_on(centre);

    // cover_test draws the distance grid for the selected octant plus a ray per octant from the
    // clicked cell. Nothing in the driver reproduces that, so the scene drives the test.
    do_key(key_mode_tests);
    tests().switch_to(floormat::tests::Test::cover);
    set_cursor_at(centre);
    // The test reads cursor_state().point() and quietly does nothing when it is empty.
    (void)cursor_point();
    // Mouse-up, not down: cover_test::handle_mouse_click returns early while is_down.
    set_mouse_button_state(mouse_button_left, true);
    set_mouse_button_state(mouse_button_left, false);
    co_yield {};

    // Stepping the selection is also what forces the next fill -- update_post() runs
    // ensure_octant() on whatever is selected before it background-fills anything else. Eight of
    // the 32 are aligned and sweep the pass bitmap with a recurrence; the other 24 are 16384
    // raycasts each, which is the bulk of what this scene costs.
    uint32_t built = 0;
    for (auto k = 0u; k < Cover::octant_count; k++)
    {
        const auto t0 = Time::now();
        do
            co_yield {};
        while (Time::to_seconds(Time::now() - t0) < octant_seconds);
        auto val = tests().current_test->advance(*this, {});
        fm_assert(val.type == tests::base_test::ValueType::u32);
        built |= val.u32;
    }
    // Every octant was selected once and each selection is ensured on the next frame, so a gap
    // here means a fill silently failed rather than that the scene ran short.
    fm_assert_equal((uint32_t)-1, built);

    fm_debug("cover: %u octants at %.1f s each, all built", Cover::octant_count,
             (double)octant_seconds);

    // Left alive, cover_test keeps calling fill_next_unfilled() every frame of whatever scene
    // comes next -- tests_post_update() is gated on current_test alone, not the editor mode.
    tests().switch_to(floormat::tests::Test::none);
    do_key(key_mode_none);
    co_yield {};
}

task app::scene_lightmap()
{
    // All nine testable chunks. Three lights per chunk cost the same per frame as one -- the two
    // shader passes are trivial at this image size -- so the scene's mass is frames, and every one
    // of them rebuilds the occlusion mesh and runs all 48 lights of the block.
    //
    // --driver forces vsync off, so frames are not wall-clock paced. 360 of them is about a second
    // at the 2.7 ms this measures at 1440x1440, which is long enough to look at each preview.
    constexpr uint32_t num_tested = 9, frames_per_light = 360;

    populate_scene_lightmap();
    auto& w = M->world();
    co_yield {};

    for (auto n = 0u; n < num_tested; n++)
    {
        const auto pt = lightmap_light(n);
        center_camera_on(pt);
        tested_light_chunk = pt.chunk3();

        // No inspector windows: they open over the preview and hide the thing the scene exists to
        // show. draw_light_info()'s labels are overlay text and stay.

        // What the shadow pass is actually fed, counted with add_geometry()/add_objects()'s own
        // predicates. A lightmap that renders nothing looks exactly like one that renders fast.
        if (n == 0)
        {
            const auto ch = pt.chunk3();
            const auto ns = M->lightmap_shader().iter_bounds();
            uint32_t walls = 0, boxes = 0, lights = 0, chunks = 0;
            for (int j = ch.y - ns; j < ch.y + ns; j++)
                for (int i = ch.x - ns; i < ch.x + ns; i++)
                {
                    auto* cʹ = w.at(chunk_coords_{(int16_t)i, (int16_t)j, ch.z});
                    if (!cʹ)
                        continue;
                    chunks++;
                    for (auto k = 0u; k < TILE_COUNT; k++)
                    {
                        auto t = (*cʹ)[k];
                        if (auto atlas = t.ground_atlas(); atlas && atlas->pass_mode() == pass_mode::blocked)
                            walls++;
                        if (auto atlas = t.wall_north_atlas(); atlas && atlas->info().passability == pass_mode::blocked)
                            walls++;
                        if (auto atlas = t.wall_west_atlas(); atlas && atlas->info().passability == pass_mode::blocked)
                            walls++;
                    }
                    for (const auto& eʹ : cʹ->objects())
                    {
                        if (eʹ->type() == object_type::light)
                            lights += static_cast<const light&>(*eʹ).max_distance >= 1e-6f;
                        else if (!eʹ->is_virtual() && eʹ->pass != pass_mode::pass &&
                                 eʹ->pass != pass_mode::see_through)
                            boxes++;
                    }
                }
            fm_debug("lightmap: %u chunks, %u walls + %u boxes = %u segments, %u lights",
                     chunks, walls, boxes, (walls + boxes)*4, lights);
            fm_assert(chunks == 16 && lights >= chunks && walls > 0 && boxes > 0);
        }

        co_yield {frames_per_light};

        // A black preview and a working one cost the same wall time, so the accumulation texture
        // is read back rather than trusted. do_lightmap_test() runs from app::draw(), ahead of
        // app::update() and this coroutine, so the frames above have already filled it.
        if (n == 0)
        {
            auto img = M->lightmap_shader().accum_texture().image(0, {PixelFormat::RGBA8Unorm});
            const auto data = img.data();
            uint32_t dark = 0, sat = 0; uint64_t sum = 0;
            for (auto i = 0uz; i + 3 < data.size(); i += 4)
            {
                const uint32_t v = Math::max({(uint32_t)(uint8_t)data[i], (uint32_t)(uint8_t)data[i+1],
                                              (uint32_t)(uint8_t)data[i+2]});
                sum += v;
                dark += v < 16;
                sat += v >= 250;
            }
            const auto total = (uint32_t)(data.size()/4);
            fm_debug("lightmap: accum %ux%u, mean %.1f, dark %.1f%%, saturated %.1f%%",
                     (uint32_t)img.size().x(), (uint32_t)img.size().y(), (double)sum/total,
                     (double)dark*100/total, (double)sat*100/total);
        }
    }

    // Left set, the lightmap would keep rendering over whatever world the next scene builds.
    tested_light_chunk = {};

    fm_debug("lightmap: %u chunks tested, %u frames each", num_tested, frames_per_light);
}


task app::scene_grids()
{
    // One Pool exists per (div_size, bbox_size), so a scene touching a single shape leaves the
    // others' build paths cold. These are the three the engine builds in play: the raycast pool,
    // the pool cover rays walk, and a plain 64-px critter.
    // Pins per occupancy sample, and samples per frame. The work is the sample count -- each is
    // 16 pins and three pool builds -- and steps_per_frame does not change it. Frames are not
    // work: 84% of a fill frame is draw_world() re-emitting every stool placed so far, which grows
    // with the pin count and has nothing to do with the grids. maybe_mark_stale_all() is not
    // frame-gated (src/grid-pass.cpp:455), so a second sample in the same frame still sees the new
    // pins as stale and rebuilds for real.
    constexpr uint32_t pins_per_step = 16, steps_per_frame = 8, max_pins = 1u << 14;
    // Stop once the bitmap is this full. Past it a new pin only takes cells an earlier one had
    // already taken, while a build still walks every object in the chunk. Read off the (8,8)
    // grid, which is the one the grid test draws.
    constexpr float fill_target = .9f;
    // Cover's 8 aligned octants sweep the pass bitmap with a recurrence and are nearly free. The
    // other 24 raycast every cell -- 16384 rays each, since div_size is pinned at 8 by
    // raycast_one's assert against cover_pass_pool. Filling all 32 twice would cost more than the
    // rest of the driver, so the expensive half is sampled.
    constexpr uint32_t cover_octant_budget = 12;

    populate_scene_grids();
    auto& w = M->world();
    auto& c = w[chunk_coords_{0, 0, 0}];
    auto C = ensure_player_character(w);
    const auto centre = C->position();
    center_camera_on(centre);

    Pass::Pool pass_raycast{Pass::Params{(uint32_t)Search::div_size.x(), tile_size_xy}.validate()};
    Pass::Pool pass_cover{Pass::Params{8, 8}.validate()};
    Pass::Pool pass_critter{Pass::Params{tile_size_xy, tile_size_xy}.validate()};

    // maybe_mark_stale_all() is where the version compare and the eager ensure_passability() of
    // the chunk and its neighbours live, so it does the whole per-frame discipline for us. The
    // handle has to be taken after it: a pooled grid can be recycled by that call.
    const auto rebuild = [&](Pass::Pool& pool)
    {
        pool.maybe_mark_stale_all(w.frame_no());
        pool[c].build_if_stale(Search::never_continue());
    };

    const auto fill_ratio = [](Pass::Grid g)
    {
        const auto n = g.div_count() * g.div_count();
        uint32_t blocked = 0;
        for (auto k = 0u; k < n; k++)
            blocked += !g.bit(k);
        return (float)blocked / (float)n;
    };

    // The grid and cover tests own the grids they draw, so driving them is both the only way to
    // see this scene work and the only route the two overlays get profiled at all. Each click is
    // one step: grid_test re-extracts the chunk bitmap, cover_test fills one more octant.
    do_key(key_mode_tests);
    set_cursor_at(centre);
    // Both tests read cursor_state().point() and quietly do nothing when it is empty, so without
    // this the scene would run to completion having drawn neither overlay.
    (void)cursor_point();
    const auto step_test = [&]
    {
        // Unlike the raycast sweep, the cursor stays put on purpose. What changed between frames
        // is the world, not the cursor, and re-extracting is the whole point.
        set_mouse_button_state(mouse_button_left, true);
        set_mouse_button_state(mouse_button_left, false);
    };

    // Empty chunk first. With nothing to stop them the cover rays run their full chunk length,
    // which is the expensive end of fill_octant; after the pins go in they die immediately.
    tests().switch_to(floormat::tests::Test::cover);
    for (auto i = 0u; i < cover_octant_budget; i++)
    {
        step_test();
        co_yield {};
    }

    tests().switch_to(floormat::tests::Test::grid);
    uint32_t num_pins = 0, num_builds = 0;
    float fill = 0;
    while (fill < fill_target && num_pins < max_pins)
    {
        for (auto s = 0u; s < steps_per_frame && fill < fill_target && num_pins < max_pins; s++)
        {
            // Once per sample, not once per pin. Sixteen builds all describing near-identical
            // occupancy, each walking the whole chunk, is what made the tail crawl.
            for (auto k = 0u; k < pins_per_step; k++)
                add_grid_pin(num_pins++);
            rebuild(pass_raycast);
            rebuild(pass_cover);
            rebuild(pass_critter);
            num_builds += 3;
            fill = fill_ratio(pass_cover[c]);
        }
        step_test();
        co_yield {};
    }

    // The pins are 8 px in a 1024-px chunk, so a bitmap that came back empty would mean the
    // objects never reached the RTree at all.
    pass_raycast.maybe_mark_stale_all(w.frame_no());
    fm_assert(!pass_raycast[c].is_all_empty());

    tests().switch_to(floormat::tests::Test::cover);
    for (auto i = 0u; i < cover_octant_budget; i++)
    {
        step_test();
        co_yield {};
    }

    fm_debug("grids: %u pins to %.0f%% full of %.0f%%, %u pass builds over 3 pools, "
             "%u cover octants of %u either side",
             num_pins, (double)fill*100, (double)fill_target*100, num_builds,
             cover_octant_budget, Cover::octant_count);

    // Test::none, not just key_mode_none. tests_post_update() is gated on current_test alone
    // (editor/update.cpp:307) and never on the editor mode, and cover_test::update_post() runs
    // fill_next_unfilled() -- up to 16384 raycasts -- on every frame it holds a result. Left
    // alive it also followed this scene into the next one's world.
    tests().switch_to(floormat::tests::Test::none);
    do_key(key_mode_none);

    // Steady state: nothing added, no pool marked stale, no test stepped. What a frame costs once
    // the world stops changing, which the fill loop cannot say -- each of its frames carries 16
    // object creations and three full pass builds.
    //
    // Timed off the wall clock because the frame count is the unknown. smoothed_fps() cannot
    // serve: under --fixed-framerate the counter is fed the flag's own dt (main/draw.cpp:92), so
    // it reads back 60 whatever the frame took.
    constexpr float idle_seconds = 3;
    const auto idle_t0 = Time::now();
    uint32_t idle_frames = 0;
    Ns idle_ns{};
    do
    {
        co_yield {};
        idle_frames++;
        idle_ns = Time::now() - idle_t0;
    } while (Time::to_seconds(idle_ns) < idle_seconds);

    const auto idle_ms = (double)Time::to_milliseconds(idle_ns);
    fm_debug("grids: idle %u frames in %.2f s over %u objects, %.2f ms/frame, %.1f fps",
             idle_frames, idle_ms/1000, num_pins, idle_ms/idle_frames,
             (double)idle_frames*1000/idle_ms);
}

task app::scene_object_ids()
{
    constexpr uint32_t num_objects = 1u << 18;
    constexpr uint32_t per_chunk = TILE_COUNT;
    constexpr uint32_t chunks_per_row = 32;
    // Centred on the origin, where it used to sit far outside get_draw_bounds(). Only what the
    // draw bounds cover is updated and drawn -- a screenful of lights against a quarter million in
    // the table -- and watching those go out is the only sign the scene is doing anything.
    constexpr int chunk_x0 = -(int)(chunks_per_row/2), chunk_y0 = chunk_x0;
    // Ground under the middle of the farm, which is all the draw bounds ever reach. Lights are
    // virtual, so without it the sprites hang over nothing.
    constexpr int16_t ground_radius = 3;
    // An eighth, not a quarter: the rounds are what the scene samples the table at, and this
    // doubles how many of them there are between full and empty.
    constexpr uint32_t kill_divisor = 8;

    reset_world();
    auto& w = M->world();
    auto ground = loader.ground_atlas("metal1");
    for (int16_t cy = -ground_radius; cy <= ground_radius; cy++)
        for (int16_t cx = -ground_radius; cx <= ground_radius; cx++)
        {
            auto& c = w[chunk_coords_{cx, cy, 0}];
            for (auto k = 0u; k < TILE_COUNT; k++)
                c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };
            c.mark_modified();
        }
    center_camera_on(point{});
    M->reset_fps();

    // Whatever reset_world() left the counter at. make_id() hands out ++counter, so this is the
    // id the first object below gets.
    const auto id0 = w.object_counter() + 1;
    object_id id1 = 0;
    Array<object_id> live{NoInit, num_objects};

    for (auto i = 0u; i < num_objects; i++)
    {
        const auto n = i / per_chunk;
        const chunk_coords_ ch{(int16_t)(chunk_x0 + (int)(n % chunks_per_row)),
                               (int16_t)(chunk_y0 + (int)(n / chunks_per_row)), 0};
        light_proto p;
        // A zero bbox keeps the object out of the RTree, which nothing here is measuring.
        p.bbox_size = {};
        live[i] = w.make_object<light>(w.make_id(), {ch, local_coords{i % per_chunk}}, p)->id;
        id1 = Math::max(id1, live[i]);
    }
    fm_assert_equal((size_t)num_objects, (size_t)(id1 - id0 + 1));
    co_yield {};

    // Fisher-Yates once, then each round kills the tail. Taking a contiguous id range instead
    // would erase whole probe sequences at a time and measure a case that never happens.
    uint64_t rng = 0x9e3779b97f4a7c15u;
    for (auto i = num_objects; i-- > 1; )
    {
        rng = rng*6364136223846793005u + 1442695040888963407u;
        const auto j = (uint32_t)((rng >> 32) % (i + 1));
        const auto tmp = live[i]; live[i] = live[j]; live[j] = tmp;
    }

    uint32_t num_live = num_objects, rounds = 0;

    for (;;)
    {
        uint32_t hits = 0;
        for (auto id = id0; id <= id1; id++)
            if (w.find_object(id))
                hits++;
        fm_assert_equal(num_live, hits);
        rounds++;
        co_yield {};

        if (num_live == 0)
            break;
        // An eighth of seven is none, and the loop would never end.
        const auto num_killed = Math::max(num_live/kill_divisor, 1u);
        for (auto i = num_live - num_killed; i < num_live; i++)
        {
            auto o = w.find_object(live[i]);
            fm_assert(o);
            // kill_object() deletes the object out from under this bptr on purpose: the chunk
            // owns the lifetime, and a borrowed pointer observes the death instead of delaying it.
            o->chunk().kill_object(o->index());
        }
        num_live -= num_killed;
    }

    fm_debug("object_ids: %u objects, ids %zu..%zu, %u rounds, %zu lookups", num_objects,
             (size_t)id0, (size_t)id1, rounds, (size_t)rounds * num_objects);
}


void app::driver_draw_overlay()
{
    auto& D = *_driver;
    if (!D.running)
        return;

    ImDrawList& draw = *ImGui::GetForegroundDrawList();

    // Whole route, every frame of the pan. The camera reaches four windows of a route tens of
    // thousands of pixels long, so without the line there is nothing saying where the rest runs.
    if (!D.route.isEmpty())
    {
        const auto route_color = ImGui::ColorConvertFloat4ToU32({0, 1, .25f, 1}),
                   goal_color  = ImGui::ColorConvertFloat4ToU32({1, .3f, 0, 1});
        const auto n = (uint32_t)D.route.size();
        auto prev = point_screen_pos(D.route.front());
        draw.AddCircle({prev.x(), prev.y()}, 9, route_color, 0, 3);
        for (auto i = 1u; i < n; i++)
        {
            const auto p = point_screen_pos(D.route[i]);
            draw.AddLine({prev.x(), prev.y()}, {p.x(), p.y()}, route_color, 3);
            prev = p;
        }
        // The goal is where the pan is headed and is off-screen for most of it.
        constexpr float arm = 11;
        draw.AddLine({prev.x()-arm, prev.y()-arm}, {prev.x()+arm, prev.y()+arm}, goal_color, 3);
        draw.AddLine({prev.x()-arm, prev.y()+arm}, {prev.x()+arm, prev.y()-arm}, goal_color, 3);

    }

    if (D.num_rays == 0)
        return;

    // Cells first, rays over them: 32 rays' worth of sampled cells is a lot of box to read a
    // line against.
    for (auto i = 0u; i < D.num_rays; i++)
        draw_raycast_diag(*this, D.diags[i]);
    // 32 endpoint dots at raycast_test's size bury the pin field they are drawn over.
    constexpr float dot_size = .75f;
    for (auto i = 0u; i < D.num_rays; i++)
        draw_raycast_line(*this, D.rays[i], dot_size);
}


void app::driver_start()
{
    M->set_events_ignored(true);
    _driver->running = true;
    _driver->scene_index = 0;
    _driver->pass_index = 0;
    _driver->frames_run = 0;
}

// Stops the run when a scene finds the world in a state it cannot proceed from. Returns true
// if the editor was handed back rather than quit, so the caller can set up something worth
// looking at.
//
// A profile run must still exit: the instrumented binary writes its profile from atexit, so
// hanging there yields no profile at all and any script wrapping it hangs with it. Otherwise
// the editor stays on screen. get_key_modifiers() is the only thing a run overrides and it
// keys off `running`, so clearing that restores interactivity with no teardown of its own.
bool app::driver_stop(StringView why)
{
    auto& D = *_driver;
    if (!D.running)
        return false;
    D.running = false;
    release_all_input();
    M->set_events_ignored(false);
    ERR_nospace << "driver stopped: " << why;
    if (M->settings().driver == driver_mode::profile)
    {
        M->quit(1);
        return false;
    }
    return true;
}

void app::driver_tick()
{
    auto& D = *_driver;
    if (!D.running)
        return;

    const auto& Scenes = scenes();

    if (D.frames_run == 0)
    {
        // driver_start() runs from app's ctor, before main_impl::exec() sizes anything, so the
        // window can only be pinned here.
        M->resize_window(M->settings().resolution);
        const auto size = M->window_size();
        std::printf("driver: %dx%d framebuffer, events ignored\n", size.x(), size.y());
        std::fflush(stdout);

        const auto list = StringView{M->settings().driver_scenes};
        // "all" is the default. "none" runs no scene at all, which is the driver's own
        // per-frame overhead measured against a run that does nothing.
        if (D.scene_mask == (uint32_t)-1)
            D.scene_mask = 0;
        for (auto name : list.splitWithoutEmptyParts(','))
        {
            name = name.trimmed();
            uint32_t i = 0;
            while (i < Scenes.size() && Scenes[i].name.exceptPrefix("scene_"_s) != name)
                i++;
            fm_assert(i < Scenes.size()); // name not found
            D.scene_mask |= 1u << i;
        }
    }

    D.frames_run++;

    if (D.scene_task.done())
    {
        if (D.scene_index > 0)
        {
            const auto name = Scenes[D.scene_index-1].name.exceptPrefix("scene_"_s);
            // Held input across a co_yield is normal -- that is how dragging works. Held input
            // across a scene boundary is a bug. Warn rather than assert, because abort() skips
            // atexit, which is where an instrumented run writes its profile.
            if (D.held_buttons != mouse_button_none || D.mods != 0 || keys_->any() ||
                _editor->is_dragging())
                fm_warn("driver: scene '%s' leaked held input", name.data());
            release_all_input();
            std::printf("%-24s%12.3f ms %7u frames  pass %u\n", name.data(),
                        (double)Time::to_milliseconds(D.scene_started.update()),
                        D.frames_run - D.scene_first_frame, D.pass_index + 1);
            std::fflush(stdout);
        }
        const auto mode = M->settings().driver;
        for (;;)
        {
            while (D.scene_index < Scenes.size())
            {
                // A name given on the command line wins over the mode filter -- asking for a
                // scene by name and having it silently skipped would be worse than useless.
                const auto want = D.scene_mask != (uint32_t)-1
                                  ? (D.scene_mask & 1u << D.scene_index) != 0
                                  : mode == driver_mode::all || Scenes[D.scene_index].mode == mode;
                if (want)
                    break;
                D.scene_index++;
            }
            if (D.scene_index < Scenes.size())
                break;
            if (++D.pass_index >= M->settings().driver_repeat)
            {
                D.running = false;
                return M->quit(0);
            }
            D.scene_index = 0;
        }
        DBG << ">>> scene:" << Scenes[D.scene_index].name;
        D.scene_task = (this->*Scenes[D.scene_index].fn)();
        D.scene_index++;
        D.scene_started = Time::now();
        D.scene_first_frame = D.frames_run;
    }

    D.scene_task.tick();
}

} // namespace floormat
