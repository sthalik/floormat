#include "app.hpp"
#include "run.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/debug.hpp"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-image.hpp"
#include "src/tile-constants.hpp"
#include "src/nanosecond.inl"
#include "src/point.inl"
#include <mg/Functions.h>

namespace floormat {
namespace {

constexpr Ns dt_60hz = Second / 60;
constexpr chunk_coords_ ch{0, 0, 0};
constexpr uint32_t settle_frames = 60, walk_frames = 60, min_slide = 16;
constexpr uint8_t line_min = 4, line_max = 12;
constexpr uint8_t npc_bbox = 32;

// arrows_to_dir() is screen-space: a world diagonal is one key, a world cardinal is two.
constexpr critter::move_s key_S  {.L = true, .D = true};
constexpr critter::move_s key_W  {.L = true, .U = true};
constexpr critter::move_s key_SW {.L = true};
constexpr critter::move_s key_SE {.D = true};
constexpr critter::move_s key_NW {.U = true};

struct slide_case
{
    StringView name;
    critter::move_s keys;
    Vector2i sign;
};

constexpr slide_case along_north[] = {
    { "SW"_s, key_SW, {-1, 0} },
    { "SE"_s, key_SE, { 1, 0} },
};

constexpr slide_case along_west[] = {
    { "NW"_s, key_NW, {0, -1} },
    { "SW"_s, key_SW, {0,  1} },
};

Vector2i press(const bptr<critter>& C, critter::move_s k, uint32_t frames)
{
    const auto before = C->position();
    C->set_keys(k.L, k.R, k.U, k.D);
    for (auto n = 0u; n < frames; n++)
    {
        auto i = C->index();
        C->update(C, i, dt_60hz);
    }
    C->set_keys(false, false, false, false);
    return C->position() - before;
}

void place(const bptr<critter>& C, point pt)
{
    auto i = C->index();
    C->teleport_to(i, pt, rotation_COUNT);
    C->delta = 0;
    C->offset_frac = 0;
}

// A blocked frame is dropped whole, so close the gap first: a critter still a few pixels short of
// the barrier would both slide and advance.
point settle(const bptr<critter>& C, point start, critter::move_s head_on)
{
    place(C, start);
    press(C, head_on, settle_frames);
    fm_assert(C->position() != start);
    return C->position();
}

void check_head_on(StringView name, const bptr<critter>& C, point flush, critter::move_s head_on)
{
    place(C, flush);
    if (const auto d = press(C, head_on, walk_frames); d != Vector2i{})
    {
        Error{standard_error()} << "!!! fatal:" << name << "head-on moved by" << d;
        fm_assert(false);
    }
}

template<size_t N>
void check_slides(StringView name, const bptr<critter>& C, point flush, const slide_case (&slides)[N])
{
    for (const auto& s : slides)
    {
        place(C, flush);
        const auto d = press(C, s.keys, walk_frames);
        if (Math::sign(d) != s.sign || (uint32_t)Math::abs(d).sum() < min_slide)
        {
            Error{standard_error()} << "!!! fatal:" << name << s.name << "slid by" << d;
            fm_assert(false);
        }
    }
}

// A wall collider is a strip on the tile's own north/west edge (src/tile-bbox.hpp), so the
// barrier lies on the low-coordinate side of the row or column named here.
void wall_row(world& w, uint8_t y)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    for (auto x = line_min; x <= line_max; x++)
        w[ch][local_coords{x, y}].wall_north() = W;
}

void wall_col(world& w, uint8_t x)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    for (auto y = line_min; y <= line_max; y++)
        w[ch][local_coords{x, y}].wall_west() = W;
}

void pillar(world& w, local_coords tile)
{
    scenery_proto p;
    p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype   = generic_scenery_proto{};
    p.bbox_size = Vector2ub(tile_size_xy);
    p.pass      = pass_mode::blocked;
    w.make_scenery(w.make_id(), {ch, tile}, move(p));
}

void scenery_row(world& w, uint8_t y)
{
    for (auto x = line_min; x <= line_max; x++)
        pillar(w, {x, y});
}

bptr<critter> add_player(world& w, float speed)
{
    object_id id = 0;
    auto proto = Run::make_proto(speed);
    proto.bbox_size = Vector2ub(npc_bbox);
    auto C = w.ensure_player_character(id, move(proto));
    Run::mark_all_modified(w);
    w.init_scripts();
    return C;
}

// test_critter's test1 already pins a head-on stop against a wall_north, so only the deflection
// is checked here.
void test_wall_north()
{
    auto w = world();
    wall_row(w, 9);
    auto C = add_player(w, 1.f);
    check_slides("wall-north"_s, C, settle(C, {ch, {8, 8}, {}}, key_S), along_north);
    w.finish_scripts();
}

// test_critter only ever places wall_west paired with wall_north as a corner, so a regression
// making it non-blocking for pure-W motion would pass the rest of the suite.
void test_wall_west()
{
    auto w = world();
    wall_col(w, 9);
    auto C = add_player(w, 1.f);
    const auto flush = settle(C, {ch, {9, 8}, {}}, key_W);
    check_head_on("wall-west"_s, C, flush, key_W);
    check_slides("wall-west"_s, C, flush, along_west);
    w.finish_scripts();
}

// Same deflection against a full-tile blocked object rather than a wall strip. test_sweep_aabb's
// blocked scenery is a 2x2 diagonal slit, which never reaches the slide.
void test_scenery_row()
{
    auto w = world();
    scenery_row(w, 9);
    auto C = add_player(w, 1.f);
    const auto flush = settle(C, {ch, {8, 8}, {}}, key_S);
    check_head_on("scenery-row"_s, C, flush, key_S);
    check_slides("scenery-row"_s, C, flush, along_north);
    w.finish_scripts();
}

// Layout mirrored from scene_slide (editor/pgo-scenes.hpp). Global tile coords, not chunk-local;
// the wall atlas differs, so only the tile numbers carry over.
constexpr int slide_start_x = 10, slide_start_y = 0;
constexpr int slide_ledge_y = 4, slide_ledge_x0 = 5, slide_ledge_x1 = 12;
constexpr int slide_wall_x = -4, slide_wall_y0 = 8, slide_wall_y1 = 14;
constexpr int slide_floor_y = 15, slide_floor_x1 = 2;
constexpr int slide_corner_x = slide_wall_x, slide_corner_y = slide_wall_y1;

// Arithmetic shift floors; a division would truncate toward zero and put tile -1 in chunk 0.
constexpr global_coords tile_at(int x, int y)
{
    static_assert(TILE_MAX_DIM == 16);
    return { chunk_coords_{(int16_t)(x >> 4), (int16_t)(y >> 4), 0},
             local_coords{(uint8_t)(x & 15), (uint8_t)(y & 15)} };
}

void slide_wall_north(world& w, int x, int y)
{
    const auto gc = tile_at(x, y);
    w[gc.chunk3()][gc.local()].wall_north() = wall_image_proto{ loader.wall_atlas("empty"), 0 };
}

void slide_wall_west(world& w, int x, int y)
{
    const auto gc = tile_at(x, y);
    w[gc.chunk3()][gc.local()].wall_west() = wall_image_proto{ loader.wall_atlas("empty"), 0 };
}

// Polls rather than running a fixed count, so the assertion does not depend on how many frames
// the run happens to take.
point run_until_still(const bptr<critter>& C, critter::move_s k, uint32_t max_frames)
{
    constexpr uint32_t poll = 8;
    C->set_keys(k.L, k.R, k.U, k.D);
    auto last = C->position();
    for (auto n = 0u; n < max_frames; n += poll)
    {
        for (auto j = 0u; j < poll; j++)
        {
            auto i = C->index();
            C->update(C, i, dt_60hz);
        }
        const auto pos = C->position();
        if (pos == last)
            break;
        last = pos;
    }
    C->set_keys(false, false, false, false);
    return last;
}

void test_slide_into_corner()
{
    auto w = world();
    for (int x = slide_ledge_x0; x <= slide_ledge_x1; x++)
        slide_wall_north(w, x, slide_ledge_y);
    for (int y = slide_wall_y0; y <= slide_wall_y1; y++)
        slide_wall_west(w, slide_wall_x, y);
    for (int x = slide_wall_x; x <= slide_floor_x1; x++)
        slide_wall_north(w, x, slide_floor_y);

    auto C = add_player(w, 10.f);
    const auto start = point{tile_at(slide_start_x, slide_start_y), {}};
    place(C, start);

    const auto end = run_until_still(C, key_SW, 2000);
    const auto corner = point{tile_at(slide_corner_x, slide_corner_y), {}};
    if (const auto off = point::distance(end, corner); off >= tile_size_xy)
    {
        Error{standard_error()} << "!!! fatal: slide ended" << off << "px from the corner at" << end;
        fm_assert(false);
    }
    // Wedged: both alternatives to SW are blocked, so holding the key moves it nowhere at all.
    if (const auto d = press(C, key_SW, walk_frames); d != Vector2i{})
    {
        Error{standard_error()} << "!!! fatal: wedged critter moved by" << d;
        fm_assert(false);
    }
    w.finish_scripts();
}

} // namespace

void Test::test_slide()
{
    test_wall_north();
    test_wall_west();
    test_scenery_row();
    test_slide_into_corner();
}

} // namespace floormat
