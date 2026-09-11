#include "app.hpp"
#include "pgo-scenes.hpp"
#include "src/world.hpp"
#include "src/hole.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/wall-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "src/grid.hpp"
#include "src/point.inl"
#include "src/timer.hpp"
#include "src/nanosecond.hpp"
#include "compat/array-size.hpp"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"
#include "floormat/main.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include <array>
#include <cr/Array.h>
#include <mg/Vector2.h>
#include <mg/Vector4.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

constexpr inline int16_t bench_chunk_min = -8, bench_chunk_max = 8;
constexpr inline int diag_u0 = 0;
using pgo::walk_chunk_min;
using pgo::walk_chunk_max;
using pgo::walk_corridor_tile;

// Obstructions across the corridor, alternating sides, so the walk weaves instead of running
// straight down one column. A tile-sized blocked object closes its own pass cell column and no
// more, so the two sides have to overlap or a column stays open under both and the route runs
// straight through: measured, a 3-wide baffle in a 9-wide corridor left column 5 clear.
constexpr inline uint8_t walk_baffle_tiles = 5, walk_baffle_pitch = 8;

// One tile of untouched scene divides two maze cells, and it blocks the pass cells either side
// of it as well as its own, so a pitch of p leaves p-3 passable columns. Six leaves the three a
// diagonal step needs.
constexpr inline uint8_t maze_pitch = 6;
constexpr inline uint32_t maze_dim = 16;
constexpr inline int maze_tile0 = -(int)(maze_dim*maze_pitch)/2;
constexpr inline uint32_t maze_seed = 0x5eed1234u;

// The second maze. generate_maze() is a recursive backtracker with a fixed direction bias, and
// measured on its own output that leaves 2 junctions in 256 cells -- one snake corridor with
// nothing to choose, so A* walks it rather than searching it. Randomized Prim plus braiding
// leaves ~350 junctions in 1024, and start/goal are the graph diameter rather than opposite
// corners, which doubles the nodes A* expands again.
constexpr inline uint32_t maze2_dim = 32;
constexpr inline uint32_t maze2_seed = 0x5eed1234u;
constexpr inline uint32_t maze2_braid_pct = 50;
constexpr inline int maze2_tile0 = -(int)(maze2_dim*maze_pitch)/2;
constexpr inline int16_t maze2_chunk_min = (int16_t)(maze2_tile0/(int)TILE_MAX_DIM);
constexpr inline int16_t maze2_chunk_max = (int16_t)((maze2_tile0 + (int)(maze2_dim*maze_pitch))/(int)TILE_MAX_DIM);
static_assert(maze2_tile0 % (int)TILE_MAX_DIM == 0); // else the chunk range above is off by one

constexpr inline uint32_t grid_pin_seed = 0x9d1d5eedu;

// Each pattern axis below uses a period from {2,3,4,5,7} on a different linear form of the
// tile coordinate, so the combined pattern repeats no sooner than every 420 tiles.

// A tile's own N/W wall strips lie *outside* it, at [T-depth, T), so only a half-extent above
// tile_size_xy/2 reaches them; anything positive reaches the S/E neighbours'. Depth here is 32.
constexpr inline Vector2ub hole_sizes[] = {
    { uint8_t(7*tile_size_xy/4), uint8_t(tile_size_xy/2)   }, // own W, E and S neighbours'
    { uint8_t(tile_size_xy/2),   uint8_t(7*tile_size_xy/4) }, // own N, E and S neighbours'
    { uint8_t(3*tile_size_xy/2), uint8_t(3*tile_size_xy/2) }, // all four, plus the corner piece
};

struct hole_z_profile { uint8_t z_offset, height; };

// A cut reaching tile_size_z lowers the wall's top cap onto the stub instead of leaving it
// at full height, so the "eats the top" cases below look different from the rest.
constexpr inline hole_z_profile hole_z_profiles[] = {
    { 0,                        uint8_t(tile_size_z)   }, // full height, eats the top
    { 0,                        uint8_t(tile_size_z/2) }, // wall left floating above the gap
    { uint8_t(tile_size_z/2),   uint8_t(tile_size_z/2) }, // eats the top
    { uint8_t(tile_size_z/4),   uint8_t(tile_size_z/2) }, // wall above and below
    { uint8_t(2*tile_size_z/3), uint8_t(tile_size_z/3) }, // eats the top, reaching it exactly
};

// The raycast scene's obstacle field. Everything the dense scene puts in a ray's way is a full
// tile across, so the ray meets the first one head-on and the case worth measuring never comes up:
// a collider the DDA's cell rectangle catches and ray_aabb_intersection then rejects. A pin is
// small enough that a ray usually slips past, and the hit distance moves as the target does.
//
// Pitch and size are tied to scene_raycast's ray length: collisions along a ray go as
// L*pin_size/pitch^2, and at L = chunk_size_xy this pair leaves the mean walked distance around
// half a chunk. Density and ray length pull against each other, so halving the pitch to fill the
// field visibly came with halving the pin.
//
// The pass grid inflates an obstacle by (bbox_size + div_size)/2 = 40 px per side, so one pin
// blocks an 84-px square. The pitch has to clear that or the lattice leaves the DDA no clear cell
// anywhere and the bitmap's skip path never runs. The grid pins keep the larger size, since
// scene_grids counts pins against bitmap occupancy.
constexpr inline int pin_size = 4, grid_pin_size = 8, pin_pitch = 3*tile_size_xy/2,
                     pin_field = pgo::raycast_radius_max + pin_pitch;

// The lattice is cartesian and the sweep is angular, so whether a ray meets a pin comes down to
// its slope, and long runs of the sweep thread between rows and reach full length. These arcs sit
// on the sweep's own angles instead, one every pin_clump_deg. A lone pin would not do: 8 px
// subtends 0.45 deg at half a chunk while the sweep steps 0.044 deg, so it would answer 20 rays
// out of 8192. Each arc spans half its sector, leaving the other half for rays that run long.
//
// The rings of one sector share that angular span rather than tiling more of it, so only the
// nearest is ever hit. The rest are there to be in the RTree: a blocked cell that holds one entry
// exercises a different inner loop from one that holds a dozen.
//
// pin_clump_span is how much of a sector an arc walls off, and so directly how many rays run to
// full length. At 1 the circle is sealed and nothing gets through.
constexpr inline int pin_clump_deg = 15, pin_clump_r0 = (int)chunk_size_xy/4;
constexpr inline float pin_clump_span = .4f;
constexpr inline uint32_t pin_clump_rings = 7, pin_clump_seed = 0xc10b5eedu;

constexpr inline const char* const scenery_names[] = {
    "chair1", "stool1", "drawers1", "shelf1", "table4", "door1", "control panel (wall) 1",
};

// Prime count so the colour never lines up with any of the placement periods.
constexpr inline Vector4ub light_colors[] = {
    { 255,  64,  64, 255 }, { 255, 128,  32, 255 }, { 255, 192,  32, 255 },
    { 255, 255,  64, 255 }, { 192, 255,  64, 255 }, {  96, 255,  64, 255 },
    {  64, 255, 144, 255 }, {  64, 255, 224, 255 }, {  64, 224, 255, 255 },
    {  64, 160, 255, 255 }, {  64,  96, 255, 255 }, { 128,  64, 255, 255 },
    { 192,  64, 255, 255 }, { 255,  64, 224, 255 }, { 255,  64, 144, 255 },
    { 255, 224, 192, 255 }, { 192, 224, 255, 255 }, { 255, 255, 255, 255 },
    { 224, 160,  96, 255 }, { 160, 224, 160, 255 }, { 160, 160, 224, 255 },
    { 224, 224, 128, 255 }, { 224, 128, 160, 255 },
};

static_assert(array_size(light_colors) == 23);

constexpr uint32_t pmod(int32_t x, int32_t m)
{
    auto r = x % m;
    return uint32_t(r < 0 ? r + m : r);
}

uint32_t hash2(uint32_t x, uint32_t y) // same mixer as app::populate_raycast_fractal()
{
    uint32_t h = x * 0x9E3779B1u + y * 0x85EBCA77u;
    h ^= h >> 16; h *= 0xC2B2AE3Du;
    h ^= h >> 13; h *= 0x27D4EB2Du;
    h ^= h >> 16;
    return h;
}

// The lightmap image covers neighbor_count = 4 chunks per axis and iter_bounds() centres it at
// [x-2, x+1] (shaders/lightmap.cpp), so a 6x6 world is the smallest that gives every chunk of the
// 3x3 testable middle a full block to render.
constexpr inline int16_t lm_chunk_min = -3, lm_chunk_max = 2;
constexpr inline int16_t lm_test_min = -1;
constexpr inline uint32_t lm_test_dim = 3;

// Wall box edges in local tiles. A north wall sits on its own tile's -y edge, so a box whose top
// run is on row lm_box0 and bottom run on row lm_box1 encloses rows lm_box0..lm_box1-1.
constexpr inline uint8_t lm_box0 = 2, lm_box1 = 13, lm_light_tile = 7;
constexpr inline int lm_pillar_size = 12;

// A 12-px stool is 3 px of the 1024-px lightmap image, too small to occlude anything. These exist
// to be shadow casters, so they get a real footprint.
constexpr inline int lm_stool_size = 48;

// A light reaches max_distance*16 texels: add_light() multiplies tiles by TILE_SIZE2.sum()/2 and
// then by image_size_ratio = 1024/4096. The image is 1024 texels across 4 chunks, so past half of
// it -- 2 chunks -- a light clips at the image edge whatever chunk it stands in.
constexpr inline uint8_t lm_range_max = 2*TILE_MAX_DIM;

// The palette spans 2.8x in perceived brightness, so a cyan and a deep blue at the same alpha do
// not read as the same light. light_color.a is a plain multiplier in the fragment shader; scaling
// it by the inverse luma lands them all at a common brightness.
constexpr inline float lm_light_luma = 130;

constexpr uint8_t lm_light_alpha(Vector4ub c)
{
    const auto luma = .2126f*c.x() + .7152f*c.y() + .0722f*c.z();
    const auto a = lm_light_luma*255/luma;
    return a >= 255 ? (uint8_t)255 : (uint8_t)a;
}

// A run of walls in local tiles. north=true is a line of north walls running +x from `at`,
// otherwise west walls running +y. A north wall sits on its own tile's -y edge and a west wall on
// its -x edge, so a west run at column x separates x-1 from x.
struct lm_wall_run { Vector2ub at; uint8_t len; bool north; };

// range is in tiles, radius in pixels. Nonzero radius makes the light an area source, which is
// what puts a penumbra on a shadow edge. Brightness is not here -- see lm_light_alpha.
struct lm_light_spec { Vector2ub at; uint8_t range; uint8_t radius; light_falloff falloff; };

// One chunk's lights and the obstructions they throw against. Three lights each: a fourth is a
// third more GPU work in a scene already GPU-bound on 48 lights x 2 full-image passes.
struct lm_layout { lm_light_spec lights[3]; lm_wall_run walls[2]; Vector2ub stools[4]; };

// Five of them against four motifs, on periods that do not lock in step (3cx+cy mod 5 against
// cx+2cy mod 4), so a 16-chunk preview block shows several combinations instead of one repeated.
// Two of the five put a pair of lights within a third of their range of each other, which is
// where the additive blend of two colours is actually visible; L2 runs a wall between such a pair.
// Every light keeps clear of the motifs' own geometry: columns 4 and 11 (colonnade), rows and
// columns 2 and 13 (room box), and the {2,5,8,11} square lattice.
constexpr inline lm_layout lm_layouts[] = {
    { // spread -- three separate pools, the old arrangement
      { {{3,3}, 8, 0, light_falloff::quadratic}, {{12,5}, 8, 24, light_falloff::linear},
        {{7,12}, 7, 72, light_falloff::quadratic} },
      { {{5,8}, 8, true}, {{9,10}, 5, false} },
      { {6,5}, {10,9}, {4,10}, {13,12} } },
    { // an overlapping pair three tiles apart, and one on its own
      { {{6,6}, 9, 24, light_falloff::quadratic}, {{9,7}, 9, 24, light_falloff::quadratic},
        {{12,12}, 6, 0, light_falloff::linear} },
      { {{3,11}, 11, true}, {{3,3}, 7, false} },
      { {7,9}, {11,6}, {5,13}, {12,9} } },
    { // a pair split by a wall, so the two colours meet along a hard edge
      { {{5,7}, 9, 72, light_falloff::quadratic}, {{12,8}, 9, 24, light_falloff::quadratic},
        {{14,4}, 6, 0, light_falloff::linear} },
      { {{8,2}, 12, false}, {{3,12}, 5, true} },
      { {6,11}, {10,5}, {13,10}, {4,4} } },
    { // an L of walls, with one light inside the corner and one outside it
      { {{7,8}, 10, 24, light_falloff::quadratic}, {{14,10}, 7, 0, light_falloff::linear},
        {{3,12}, 7, 72, light_falloff::quadratic} },
      { {{3,5}, 10, true}, {{12,5}, 8, false} },
      { {6,10}, {9,10}, {10,3}, {14,3} } },
    { // all three clustered, so every stool between them throws three coloured shadows
      { {{6,7}, 9, 24, light_falloff::quadratic}, {{9,6}, 9, 0, light_falloff::quadratic},
        {{8,10}, 9, 72, light_falloff::quadratic} },
      { {{2,13}, 12, true}, {{2,3}, 10, false} },
      { {7,8}, {11,9}, {5,4}, {12,12} } },
};

constexpr bool lm_layouts_in_bounds()
{
    for (const auto& L : lm_layouts)
    {
        for (const auto& l : L.lights)
            if (l.range >= lm_range_max || l.at.x() >= TILE_MAX_DIM || l.at.y() >= TILE_MAX_DIM)
                return false;
        for (const auto& r : L.walls)
        {
            const uint32_t ex = r.at.x() + (r.north ? r.len-1u : 0u),
                           ey = r.at.y() + (r.north ? 0u : r.len-1u);
            if (ex >= TILE_MAX_DIM || ey >= TILE_MAX_DIM)
                return false;
        }
        for (auto s : L.stools)
            if (s.x() >= TILE_MAX_DIM || s.y() >= TILE_MAX_DIM)
                return false;
    }
    return true;
}

static_assert(lm_layouts_in_bounds());

struct scene_assets
{
    std::array<bptr<ground_atlas>, 2> ground;
    std::array<bptr<wall_atlas>, 3> walls;
    std::array<scenery_proto, static_array_size<decltype(scenery_names)>> scenery;
};

scene_assets load_assets()
{
    scene_assets a;
    // "texel" is pass-mode blocked, so it can't stand in for either of these.
    a.ground[0] = loader.ground_atlas("floor-tiles");
    a.ground[1] = loader.ground_atlas("metal1");
    // Only test1..3 share depth 32; "empty" also lacks the side/top/corner groups entirely,
    // which would leave the hole-cut paths this scene exists to exercise unreached.
    a.walls[0] = loader.wall_atlas("test1", loader_policy::warn);
    a.walls[1] = loader.wall_atlas("test2", loader_policy::warn);
    a.walls[2] = loader.wall_atlas("test3", loader_policy::warn);
    for (auto i = 0u; i < array_size(scenery_names); i++)
        a.scenery[i] = loader.scenery(scenery_names[i]);
    return a;
}

scenery_proto pin_proto(int size)
{
    auto p = loader.scenery("stool1");
    p.bbox_offset = {};
    p.bbox_size = Vector2ub{(uint8_t)size};
    p.pass = pass_mode::blocked;
    return p;
}

// Jittered so no ray runs down a lattice line: an axis-aligned or 45-degree ray through an
// aligned lattice either meets every pin head-on or threads all of them, and neither is the
// grazing case.
void generate_raycast_pins(world& w)
{
    auto ground = loader.ground_atlas("metal1");
    // World origin is a chunk corner rather than a centre, so a symmetric pin field needs an
    // asymmetric chunk range to sit on.
    const auto ch0 = point::normalize_coords(point{}, Vector2i{-pin_field}).chunk3(),
               ch1 = point::normalize_coords(point{}, Vector2i{ pin_field}).chunk3();
    for (int16_t cy = ch0.y; cy <= ch1.y; cy++)
        for (int16_t cx = ch0.x; cx <= ch1.x; cx++)
        {
            auto& c = w[chunk_coords_{cx, cy, 0}];
            for (auto k = 0u; k < TILE_COUNT; k++)
                c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };
        }

    const auto proto = pin_proto(pin_size);

    const auto put_pin = [&](Vector2i at)
    {
        // A pin standing on the player would stop every ray at zero distance.
        if (Math::abs(at).max() < tile_size_xy/2)
            return;
        const auto pt = point::normalize_coords(point{}, at);
        auto p = proto;
        p.offset = pt.offset();
        w.make_scenery<false>(w.make_id(), pt.coord(), move(p));
    };

    for (int y = -pin_field; y <= pin_field; y += pin_pitch)
        for (int x = -pin_field; x <= pin_field; x += pin_pitch)
        {
            const auto h = hash2((uint32_t)x, (uint32_t)y);
            put_pin({x + (int)(h % pin_pitch)/2 - pin_pitch/4,
                     y + (int)(h/pin_pitch % pin_pitch)/2 - pin_pitch/4});
        }

    constexpr uint32_t num_clumps = 360/pin_clump_deg;
    constexpr float sector = 2.f * Math::Constants<float>::pi() / (float)num_clumps;

    for (auto k = 0u; k < num_clumps; k++)
        for (auto ring = 0u; ring < pin_clump_rings; ring++)
        {
            // Radius varies per arc so the sweep meets them at a spread of distances rather than
            // all at once. Under pin_clump_r0 a ray stops before the DDA crosses a chunk edge.
            const auto h = hash2(k*pin_clump_rings + ring, pin_clump_seed);
            const auto r = (float)(pin_clump_r0 + (int)(h % (uint32_t)(pin_field - pin_clump_r0)));
            // Pins spaced by their own width, so the arc has no gap for a ray to pass through.
            const auto n = (uint32_t)(r * sector*pin_clump_span / (float)pin_size) + 1;
            for (auto i = 0u; i < n; i++)
            {
                const auto theta = Rad{sector*((float)k - pin_clump_span/2
                                               + pin_clump_span*(float)i/(float)n)};
                put_pin(Vector2i(Vector2{Math::cos(theta), Math::sin(theta)} * r));
            }
        }

    for (auto& c : w.chunks())
    {
        c.sort_objects();
        c.mark_modified();
    }
}

uint32_t generate_chunk(world& w, chunk_coords_ ch, const scene_assets& a, bool walls)
{
    auto& c = w[ch];
    const auto chunk_parity = pmod(ch.x + ch.y, 2);
    const auto& ground = a.ground[chunk_parity];

    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };

    uint32_t objects = 0;

    for (uint8_t ly = 0; ly < TILE_MAX_DIM; ly++)
        for (uint8_t lx = 0; lx < TILE_MAX_DIM; lx++)
        {
            // Shifting by a different coprime multiple of z per axis keeps stacked Z levels
            // from being copies of each other.
            const auto px = int32_t(ch.x)*int32_t(TILE_MAX_DIM) + lx + 7*ch.z;
            const auto py = int32_t(ch.y)*int32_t(TILE_MAX_DIM) + ly + 11*ch.z;
            const local_coords at{lx, ly};

            // variant -1 picks the frame by hashing the coordinate, giving per-tile variety
            // without a fourth pattern axis to keep coprime.
            if (walls)
            {
                c[at].wall_north() = { a.walls[hash2((uint32_t)px, (uint32_t)py) % 3], (variant_t)-1 };
                c[at].wall_west()  = { a.walls[hash2((uint32_t)py, (uint32_t)px) % 3], (variant_t)-1 };
            }

            // TILE_MAX_DIM is even, so a plain tile checkerboard runs straight through a chunk
            // boundary. Folding in the chunk coordinate inverts it per chunk, marking the seam.
            if (pmod(px + py + ch.x + ch.y, 2) == 0)
            {
                const auto& z = hole_z_profiles[pmod(2*px + py, 5)];
                hole_proto p;
                p.bbox_size = hole_sizes[pmod(px + 2*py, 3)];
                p.z_offset  = z.z_offset;
                p.height    = z.height;
                w.make_object<hole, false>(w.make_id(), {ch, at}, p);
                objects++;
            }

            {
                auto p = a.scenery[pmod(3*px + 5*py, 7)];
                w.make_scenery<false>(w.make_id(), {ch, at}, move(p));
                objects++;
            }

            // Every fourth tile. This form forces px even, so whether the light lands on a
            // hole tile flips with each row rather than being fixed for the whole scene.
            if (pmod(px + 2*py, 4) == 0)
            {
                light_proto p;
                p.color = light_colors[hash2((uint32_t)px ^ 0x5a5au, (uint32_t)py) % array_size(light_colors)];
                // Tiles, not pixels: lightmap_shader::add_light() multiplies by TILE_SIZE2.sum()/2.
                // Zero would drop the light before the falloff is read.
                p.max_distance = 3;
                p.falloff = light_falloff::linear;
                w.make_object<light, false>(w.make_id(), {ch, at}, p);
                objects++;
            }
        }

    // Objects go in unsorted above, because at ~450 per chunk a positioned insert each would
    // cost more than the rest of the generator put together.
    c.sort_objects();
    c.mark_modified();
    return objects;
}

// Cuts a north-south corridor. Only N and W walls exist, so the corridor's sides are the W wall
// of its first column and the one of the column past its last. Those stay -- they are the walls.
// Out goes what blocks travel along it: the N walls, the interior W walls, and the objects.
//
// A corridor `width` tiles wide leaves `width-2` passable columns, which is also why
// populate_labyrinth()'s step of 4 leaves 2. src/grid-pass.cpp:211-225 inflates a W wall on tile
// i0 across cells i0-1..i0, so each side wall eats one column of the interior. Width 1 and 2 leave
// none and cannot route at any div_size; width 5 is the first to leave 3, the minimum for a
// diagonal step, since is_passable_between_diag() tests the two off-axis cells too.
void carve_corridor(world& w, int16_t cx, uint8_t start_tile, uint8_t width, int16_t cy_min, int16_t cy_max)
{
    fm_assert(width > 0 && start_tile + width < TILE_MAX_DIM);
    // The scene is generated without walls so there is something to look at, so the corridor's
    // two sides are put here instead of being what is left standing after the cut.
    auto wall = loader.wall_atlas("test1", loader_policy::warn);

    for (int16_t cy = cy_min; cy <= cy_max; cy++)
    {
        auto& c = w[chunk_coords_{cx, cy, 0}];
        for (uint8_t ly = 0; ly < TILE_MAX_DIM; ly++)
        {
            c[local_coords{start_tile, ly}].wall_west() = { wall, (variant_t)-1 };
            c[local_coords{(uint8_t)(start_tile + width), ly}].wall_west() = { wall, (variant_t)-1 };
        }
        // Backwards because arrayRemove() shifts the tail down.
        for (auto i = (uint32_t)c.objects().size(); i-- > 0; )
            if (auto lx = c.objects()[i]->coord.local().x; lx >= start_tile && lx < start_tile + width)
                c.kill_object(i);
        c.mark_modified();
    }
}

// Cuts a diagonal band out of the dense scene: walls and objects go, the ground stays. The band
// runs along +x+y, which projects to straight down the screen, so its edges sit beside it at equal
// depth instead of in front of it. That is the whole reason a diagonal cut is visible here and a
// north-south one is not -- for that one, every tile a step east is a 192 px wall drawn in front,
// and a wall row hides six rows behind it.
//
// half_width is set by the pass bitmap rather than by the critter. src/grid-pass.cpp:211-225
// blocks the cells around every obstacle, so a cell needs its whole 3x3 tile neighbourhood clear;
// on a 45-degree band that costs two tiles of u at each edge and leaves 2*half_width-3 passable.
// Three is the minimum a diagonal step needs, because is_passable_between_diag() tests the two
// off-axis cells as well.
void carve_diagonal(world& w, int u0, uint8_t half_width, int16_t cmin, int16_t cmax)
{
    const int h = half_width;
    constexpr int dim = (int)TILE_MAX_DIM;

    for (int16_t cy = cmin; cy <= cmax; cy++)
        for (int16_t cx = cmin; cx <= cmax; cx++)
        {
            auto* c = w.at(chunk_coords_{cx, cy, 0});
            if (!c)
                continue;
            const int u_chunk = (cx - cy)*dim;
            bool hit = false;
            for (uint8_t ly = 0; ly < TILE_MAX_DIM; ly++)
                for (uint8_t lx = 0; lx < TILE_MAX_DIM; lx++)
                {
                    const int u = u_chunk + lx - ly;
                    if (u < u0 - h || u > u0 + h)
                        continue;
                    auto t = (*c)[local_coords{lx, ly}];
                    t.wall_north() = {};
                    t.wall_west() = {};
                    hit = true;
                }
            if (!hit)
                continue;
            // Backwards because arrayRemove() shifts the tail down.
            for (auto i = (uint32_t)c->objects().size(); i-- > 0; )
            {
                const auto lc = c->objects()[i]->coord.local();
                const int u = u_chunk + lc.x - lc.y;
                if (u >= u0 - h && u <= u0 + h)
                    c->kill_object(i);
            }
            c->mark_modified();
        }
}

enum maze_flag : uint8_t { maze_seen = 1 << 0, maze_open_e = 1 << 1, maze_open_s = 1 << 2 };

// W, N, E, S. The DFS tries the first two first, which is what makes the maze adversarial: they
// point away from the goal cell at (max,max), so the trunk wanders and the branches aimed at the
// goal become the dead ends. octile_distance is admissible, so A* expands every node under the
// true cost -- and here that is nearly all of them.
constexpr inline Vector2i maze_dirs[4] = { {-1,0}, {0,-1}, {1,0}, {0,1} };

// Recursive backtracker. The maze is perfect -- no loops -- so every wrong turn is a dead end.
Array<uint8_t> generate_maze(uint32_t dim, uint32_t seed)
{
    Array<uint8_t> flags{ValueInit, dim*dim};
    Array<uint32_t> stack{NoInit, dim*dim};
    uint32_t top = 0;

    flags[0] = maze_seen;
    stack[top++] = 0;

    while (top > 0)
    {
        const auto cur = stack[top-1];
        const auto i = (int)(cur % dim), j = (int)(cur / dim);
        const auto h = hash2(cur, seed);
        // Away-from-goal pair first, the hash only choosing within each pair.
        const uint32_t order[4] = {
            h & 1 ? 1u : 0u, h & 1 ? 0u : 1u,
            h & 2 ? 3u : 2u, h & 2 ? 2u : 3u,
        };

        bool moved = false;
        for (auto k = 0u; k < 4 && !moved; k++)
        {
            const auto d = maze_dirs[order[k]];
            const auto ni = i + d.x(), nj = j + d.y();
            if (ni < 0 || nj < 0 || ni >= (int)dim || nj >= (int)dim)
                continue;
            const auto next = (uint32_t)nj*dim + (uint32_t)ni;
            if (flags[next] & maze_seen)
                continue;
            // One bit per wall, kept on the west or north cell of the pair that shares it.
            if (d.x() > 0)
                flags[cur] |= maze_open_e;
            else if (d.x() < 0)
                flags[next] |= maze_open_e;
            else if (d.y() > 0)
                flags[cur] |= maze_open_s;
            else
                flags[next] |= maze_open_s;
            flags[next] |= maze_seen;
            stack[top++] = next;
            moved = true;
        }
        if (!moved)
            top--;
    }
    return flags;
}

// The cells and their open walls lose their walls and objects; what is left standing is the
// maze. No edge wall to preserve as in carve_corridor() -- the scene around it already blocks.
// One bit per wall, kept on the west or north cell of the pair that shares it, so a tile between
// two cells is open only if the owning cell says so.
bool maze_is_open(ArrayView<const uint8_t> flags, uint32_t cells, int tile0, int gx, int gy)
{
    const int span = (int)(cells*maze_pitch);
    const int ux = gx - tile0, uy = gy - tile0;
    if (ux < 0 || uy < 0 || ux > span || uy > span)
        return false;
    const auto rx = (int)pmod(ux, maze_pitch), ry = (int)pmod(uy, maze_pitch);
    const auto cx = (ux - rx)/maze_pitch, cy = (uy - ry)/maze_pitch;
    if (rx && ry)
        return true;
    if (!rx && !ry) // the pillar where two wall lines cross is never open
        return false;
    if (!rx)
        return cx > 0 && cx < (int)cells && (flags[(uint32_t)(cy*(int)cells + cx - 1)] & maze_open_e) != 0;
    return cy > 0 && cy < (int)cells && (flags[(uint32_t)((cy-1)*(int)cells + cx)] & maze_open_s) != 0;
}

struct maze_neighbors { uint32_t cell[4]; uint32_t count; };

maze_neighbors maze_adjacent(ArrayView<const uint8_t> flags, uint32_t dim, uint32_t c)
{
    maze_neighbors r{};
    const auto i = c % dim, j = c / dim;
    if ((flags[c] & maze_open_e) && i+1 < dim)
        r.cell[r.count++] = c+1;
    if (i > 0 && (flags[c-1] & maze_open_e))
        r.cell[r.count++] = c-1;
    if ((flags[c] & maze_open_s) && j+1 < dim)
        r.cell[r.count++] = c+dim;
    if (j > 0 && (flags[c-dim] & maze_open_s))
        r.cell[r.count++] = c-dim;
    return r;
}

void maze_open_wall(ArrayView<uint8_t> flags, uint32_t dim, uint32_t a, uint32_t b)
{
    if (b == a + 1)
        flags[a] |= maze_open_e;
    else if (b + 1 == a)
        flags[b] |= maze_open_e;
    else if (b == a + dim)
        flags[a] |= maze_open_s;
    else
        flags[b] |= maze_open_s;
}

// Randomized Prim: pick uniformly from every wall on the frontier, rather than from the walls of
// the one cell just reached. That is where the junctions come from -- a DFS always extends its
// newest cell and so tends to lay down a single long corridor.
Array<uint8_t> generate_maze_braid(uint32_t dim, uint32_t seed)
{
    Array<uint8_t> flags{ValueInit, dim*dim};
    struct edge { uint32_t src, dst; };
    // A cell is pushed at most once per neighbour over the whole run, so this never overflows.
    Array<edge> frontier{NoInit, 4*dim*dim};
    uint32_t top = 0, step = 0;

    const auto push_frontier = [&](uint32_t c)
    {
        const auto i = (int)(c % dim), j = (int)(c / dim);
        for (auto k = 0u; k < 4; k++)
        {
            const auto ni = i + maze_dirs[k].x(), nj = j + maze_dirs[k].y();
            if (ni < 0 || nj < 0 || ni >= (int)dim || nj >= (int)dim)
                continue;
            const auto n = (uint32_t)nj*dim + (uint32_t)ni;
            if (!(flags[n] & maze_seen))
                frontier[top++] = { c, n };
        }
    };

    flags[0] = maze_seen;
    push_frontier(0);

    while (top > 0)
    {
        const auto i = hash2(++step, seed) % top;
        const auto e = frontier[i];
        frontier[i] = frontier[--top];
        if (flags[e.dst] & maze_seen)
            continue;
        maze_open_wall(flags, dim, e.src, e.dst);
        flags[e.dst] |= maze_seen;
        push_frontier(e.dst);
    }

    // A dead end costs A* one step in and one step back. Reconnecting half of them leaves real
    // alternative routes, so the frontier stays wide instead of collapsing onto a single path.
    for (uint32_t c = 0; c < dim*dim; c++)
    {
        const auto adj = maze_adjacent(flags, dim, c);
        if (adj.count != 1 || hash2(c, seed ^ 0xb1a1du) % 100 >= maze2_braid_pct)
            continue;
        uint32_t cand[4], num = 0;
        const auto i = (int)(c % dim), j = (int)(c / dim);
        for (auto k = 0u; k < 4; k++)
        {
            const auto ni = i + maze_dirs[k].x(), nj = j + maze_dirs[k].y();
            if (ni < 0 || nj < 0 || ni >= (int)dim || nj >= (int)dim)
                continue;
            const auto n = (uint32_t)nj*dim + (uint32_t)ni;
            if (n != adj.cell[0])
                cand[num++] = n;
        }
        if (num)
            maze_open_wall(flags, dim, c, cand[hash2(c, seed ^ 0xc0deu) % num]);
    }

    return flags;
}

uint32_t maze_farthest_from(ArrayView<const uint8_t> flags, uint32_t dim, uint32_t src)
{
    const auto count = dim*dim;
    Array<uint32_t> dist{ValueInit, count}; // 0 means unvisited, so src starts at 1
    Array<uint32_t> queue{NoInit, count};
    uint32_t head = 0, tail = 0, best = src;

    dist[src] = 1;
    queue[tail++] = src;
    while (head < tail)
    {
        const auto c = queue[head++];
        if (dist[c] > dist[best])
            best = c;
        const auto adj = maze_adjacent(flags, dim, c);
        for (auto k = 0u; k < adj.count; k++)
        {
            const auto n = adj.cell[k];
            if (dist[n])
                continue;
            dist[n] = dist[c] + 1;
            queue[tail++] = n;
        }
    }
    return best;
}

struct maze2_scene { Array<uint8_t> flags; uint32_t start, goal; };

// Cheap enough to redo per call at 1024 cells, which keeps the endpoints out of file scope.
maze2_scene generate_maze2()
{
    auto flags = generate_maze_braid(maze2_dim, maze2_seed);
    // Two BFS passes give the graph diameter. Opposite corners look further apart and are not:
    // straight-line distance is what the heuristic sees, and the diameter pair is the one that
    // leaves it most wrong.
    const auto a = maze_farthest_from(flags, maze2_dim, 0);
    const auto b = maze_farthest_from(flags, maze2_dim, a);
    return { move(flags), a, b };
}

point maze2_cell_point(uint32_t cell)
{
    constexpr int dim = (int)TILE_MAX_DIM;
    const int gx = maze2_tile0 + (int)(cell % maze2_dim)*maze_pitch + maze_pitch/2;
    const int gy = maze2_tile0 + (int)(cell / maze2_dim)*maze_pitch + maze_pitch/2;
    const auto lx = (int)pmod(gx, dim), ly = (int)pmod(gy, dim);
    return point{chunk_coords_{(int16_t)((gx - lx)/dim), (int16_t)((gy - ly)/dim), 0},
                 local_coords{(uint8_t)lx, (uint8_t)ly}, {}};
}

// Ground and walls, no scenery. The first maze is carved out of the benchmark scene and drags
// 129k objects along with it; none of that is being measured here.
void build_maze2(world& w, ArrayView<const uint8_t> flags)
{
    auto ground = loader.ground_atlas("floor-tiles");
    auto wall = loader.wall_atlas("test1", loader_policy::warn);
    constexpr int dim = (int)TILE_MAX_DIM;

    for (int16_t cy = maze2_chunk_min; cy <= maze2_chunk_max; cy++)
        for (int16_t cx = maze2_chunk_min; cx <= maze2_chunk_max; cx++)
        {
            auto& c = w[chunk_coords_{cx, cy, 0}];
            const int x0 = cx*dim, y0 = cy*dim;
            for (uint8_t ly = 0; ly < TILE_MAX_DIM; ly++)
                for (uint8_t lx = 0; lx < TILE_MAX_DIM; lx++)
                {
                    auto t = c[local_coords{lx, ly}];
                    t.ground() = { ground, variant_t((lx + ly) % ground->num_tiles()) };
                    if (maze_is_open(flags, maze2_dim, maze2_tile0, x0 + lx, y0 + ly))
                        continue;
                    t.wall_north() = { wall, (variant_t)-1 };
                    t.wall_west()  = { wall, (variant_t)-1 };
                }
            c.mark_modified();
        }
}

// The maze is built rather than carved. Walling all 289 chunks and cutting corridors out of the
// middle 36 left the camera inside a solid block: a wall frame is 192 px against 32 px of ground
// recession per tile row, so six rows of masonry stand between the eye and anything behind them
// and the scene came out an undifferentiated wall. It now generates without walls, and only the
// maze itself gets any.
//
// maze_is_open() is false along the span's own edge, which seals the block. Without that seal A*
// would leave by the nearest side and walk around the outside.
void build_maze(world& w, ArrayView<const uint8_t> flags, uint32_t cells, int16_t cmin, int16_t cmax)
{
    constexpr int dim = (int)TILE_MAX_DIM;
    const int span = (int)(cells*maze_pitch);
    auto wall = loader.wall_atlas("test1", loader_policy::warn);

    const auto in_maze = [&](int gx, int gy)
    {
        const int ux = gx - maze_tile0, uy = gy - maze_tile0;
        return ux >= 0 && uy >= 0 && ux <= span && uy <= span;
    };
    const auto is_open = [&](int gx, int gy)
    {
        return maze_is_open(flags, cells, maze_tile0, gx, gy);
    };

    for (int16_t cy = cmin; cy <= cmax; cy++)
        for (int16_t cx = cmin; cx <= cmax; cx++)
        {
            auto* c = w.at(chunk_coords_{cx, cy, 0});
            if (!c)
                continue;
            const int x0 = cx*dim, y0 = cy*dim;
            bool hit = false;
            for (uint8_t ly = 0; ly < TILE_MAX_DIM; ly++)
                for (uint8_t lx = 0; lx < TILE_MAX_DIM; lx++)
                {
                    const int gx = x0 + lx, gy = y0 + ly;
                    if (!in_maze(gx, gy))
                        continue;
                    hit = true;
                    if (is_open(gx, gy))
                        continue;
                    auto t = (*c)[local_coords{lx, ly}];
                    t.wall_north() = { wall, (variant_t)-1 };
                    t.wall_west()  = { wall, (variant_t)-1 };
                }
            if (!hit)
                continue;
            // Backwards because arrayRemove() shifts the tail down.
            for (auto i = (uint32_t)c->objects().size(); i-- > 0; )
            {
                const auto lc = c->objects()[i]->coord.local();
                const int gx = x0 + lc.x, gy = y0 + lc.y;
                if (in_maze(gx, gy) && is_open(gx, gy))
                    c->kill_object(i);
            }
            c->mark_modified();
        }
}

// The middle tile of a cell's interior. Cell 0 is at tile pitch/2, which the walls either side
// leave passable for any pitch >= 4.
point maze_cell_point(uint32_t i, uint32_t j)
{
    constexpr int dim = (int)TILE_MAX_DIM;
    const int gx = maze_tile0 + (int)i*maze_pitch + maze_pitch/2;
    const int gy = maze_tile0 + (int)j*maze_pitch + maze_pitch/2;
    const auto lx = (int)pmod(gx, dim), ly = (int)pmod(gy, dim);
    return point{chunk_coords_{(int16_t)((gx - lx)/dim), (int16_t)((gy - ly)/dim), 0},
                 local_coords{(uint8_t)lx, (uint8_t)ly}, {}};
}

void generate_scene(world& w, int z_min, int z_max, bool walls)
{
    const auto a = load_assets();
    auto t = Time::now();
    uint32_t chunks = 0, objects = 0;

    for (int z = z_min; z <= z_max; z++)
    {
        for (int16_t cy = bench_chunk_min; cy <= bench_chunk_max; cy++)
            for (int16_t cx = bench_chunk_min; cx <= bench_chunk_max; cx++)
            {
                objects += generate_chunk(w, {cx, cy, (int8_t)z}, a, walls);
                chunks++;
            }
        if (z_min != z_max)
            fm_debug("scene benchmark: z=%d done, %u chunks", z, chunks);
    }

    fm_debug("scene benchmark: %u chunks, %u objects in %.1f ms",
             chunks, objects, (double)Time::to_milliseconds(t.update()));
}

void lm_pillar(world& w, chunk_coords_ ch, uint8_t lx, uint8_t ly, Vector2b off)
{
    auto p = pin_proto(lm_pillar_size);
    p.offset = off;
    w.make_scenery<false>(w.make_id(), {ch, local_coords{lx, ly}}, move(p));
}

// The layout's own obstructions, as opposed to lm_populate_chunk's, which belong to the motif.
// These sit between a fifth and a half of each light's range away -- close enough to throw a wedge
// across the lit area, far enough that the light is not walled in at its own tile.
void lm_place_obstructions(world& w, chunk& c, chunk_coords_ ch, const bptr<wall_atlas>& wall,
                           const lm_layout& L)
{
    for (const auto& r : L.walls)
        for (uint8_t k = 0; k < r.len; k++)
        {
            auto t = c[local_coords{(uint8_t)(r.at.x() + (r.north ? k : 0)),
                                    (uint8_t)(r.at.y() + (r.north ? 0 : k))}];
            if (r.north)
                t.wall_north() = { wall, (variant_t)-1 };
            else
                t.wall_west() = { wall, (variant_t)-1 };
        }

    for (auto at : L.stools)
        w.make_scenery<false>(w.make_id(), {ch, local_coords{at.x(), at.y()}},
                              pin_proto(lm_stool_size));
}

// Four motifs, so one preview shows four different things the shadow pass does rather than four
// copies of one. Which chunk gets which is a band pattern and not a hash, so they stay tellable
// apart in the preview.
void lm_populate_chunk(world& w, chunk_coords_ ch, const bptr<wall_atlas>& wall, uint32_t theme)
{
    auto& c = w[ch];
    switch (theme)
    {
    case 0: { // a room, with a doorway the light escapes through onto whatever is outside
        constexpr uint8_t mid = (lm_box0 + lm_box1)/2;
        const auto door = pmod(ch.x*5 + ch.y*3, 4);
        for (uint8_t t = lm_box0; t < lm_box1; t++)
        {
            const bool gap = t >= mid-1 && t <= mid+1;
            if (!(gap && door == 0)) c[local_coords{t, lm_box0}].wall_north() = { wall, (variant_t)-1 };
            if (!(gap && door == 1)) c[local_coords{t, lm_box1}].wall_north() = { wall, (variant_t)-1 };
            if (!(gap && door == 2)) c[local_coords{lm_box0, t}].wall_west() = { wall, (variant_t)-1 };
            if (!(gap && door == 3)) c[local_coords{lm_box1, t}].wall_west() = { wall, (variant_t)-1 };
        }
        // Off the light's own row and column, so each throws its shadow a different way.
        lm_pillar(w, ch, 4, 5, {});
        lm_pillar(w, ch, 10, 4, {});
        lm_pillar(w, ch, 5, 11, {});
        lm_pillar(w, ch, 11, 10, {});
        break;
    }
    case 1: // two colonnades: the light stands between them and the shadows run out in parallel
        for (uint8_t ly = 3; ly < 14; ly++)
        {
            lm_pillar(w, ch, 4, ly, {});
            lm_pillar(w, ch, 11, ly, {});
        }
        break;
    case 2: // staggered wall runs, so the wedge from one lands on the lit gap of the next
        for (uint8_t s = 0; s < 3; s++)
        {
            const uint8_t row = (uint8_t)(3 + s*5), x0 = (uint8_t)(s % 2 ? 2 : 8);
            for (uint8_t lx = x0; lx < x0 + 6; lx++)
                c[local_coords{lx, row}].wall_north() = { wall, (variant_t)-1 };
        }
        break;
    default: // jittered pillar field -- a fan of shadow fingers, no two the same length
        for (uint8_t ly = 2; ly < 14; ly = (uint8_t)(ly + 3))
            for (uint8_t lx = 2; lx < 14; lx = (uint8_t)(lx + 3))
            {
                const auto h = hash2(lx ^ (uint32_t)(ch.x*32), ly ^ (uint32_t)(ch.y*32));
                lm_pillar(w, ch, lx, ly, {(int8_t)(h % 33 - 16), (int8_t)(h/33 % 33 - 16)});
            }
        break;
    }
}

void generate_lightmap_scene(world& w)
{
    const auto a = load_assets();
    const auto& wall = a.walls[0];
    uint32_t n = 0;

    for (int16_t cy = lm_chunk_min; cy <= lm_chunk_max; cy++)
        for (int16_t cx = lm_chunk_min; cx <= lm_chunk_max; cx++, n++)
        {
            const chunk_coords_ ch{cx, cy, 0};
            auto& c = w[ch];
            const auto& ground = a.ground[pmod(cx + cy, 2)];
            for (auto k = 0u; k < TILE_COUNT; k++)
                c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };

            lm_populate_chunk(w, ch, wall, pmod(cx + 2*cy, 4));
            const auto& L = lm_layouts[pmod(3*cx + cy, (int32_t)array_size(lm_layouts))];
            lm_place_obstructions(w, c, ch, wall, L);

            for (auto m = 0u; m < array_size(L.lights); m++)
            {
                const auto& spec = L.lights[m];
                const auto k = n * (uint32_t)array_size(L.lights) + m;
                light_proto p;
                // 7 is coprime with 23, so neighbouring lights never share a colour -- and a pair
                // placed to overlap gets two hues a third of the wheel apart, which is what makes
                // the blend between them read as a mix rather than as a brighter blob.
                const auto rgb = light_colors[k * 7 % array_size(light_colors)];
                p.color = { rgb.x(), rgb.y(), rgb.z(), lm_light_alpha(rgb) };
                // A constant light is a flat full-brightness disc of TILE_MAX_DIM tiles whatever
                // its range -- add_light() hardcodes that -- so a second one anywhere near it
                // whites out the preview. One, in the corner chunk only the first block reaches.
                p.falloff = cx == lm_chunk_min && cy == lm_chunk_min && m == 0
                            ? light_falloff::constant : spec.falloff;
                p.max_distance = spec.range;
                p.radius = spec.radius;
                w.make_object<light, false>(w.make_id(),
                                            {ch, local_coords{spec.at.x(), spec.at.y()}}, p);
            }

            c.sort_objects();
            c.mark_modified();
        }
}

void carve_corridor_baffles(world& w, int16_t cx, uint8_t start_tile, uint8_t width,
                            int16_t cy_min, int16_t cy_max)
{
    // A tile-sized collider rather than a wall run: a wall frame is 192 px tall and would hide
    // the six tile rows behind every baffle, which is what made the walled walk scene unwatchable.
    const auto proto = pin_proto(tile_size_xy);
    const auto rows = (uint32_t)(cy_max - cy_min + 1) * TILE_MAX_DIM;

    // Nothing in the first or last pitch, so the critter reaches its start column and gets back to
    // the goal's without a baffle in the way.
    for (auto row = (uint32_t)walk_baffle_pitch; row + walk_baffle_pitch < rows;
         row += walk_baffle_pitch)
    {
        const bool east = (row / walk_baffle_pitch) & 1;
        const auto x0 = (uint8_t)(east ? start_tile + width - walk_baffle_tiles : start_tile + 1);
        const auto ch = chunk_coords_{cx, (int16_t)(cy_min + (int)(row / TILE_MAX_DIM)), 0};
        const auto ly = (uint8_t)(row % TILE_MAX_DIM);
        for (uint8_t k = 0; k < walk_baffle_tiles; k++)
        {
            auto p = proto;
            w.make_scenery<false>(w.make_id(), global_coords{ch, local_coords{(uint8_t)(x0 + k), ly}},
                                  move(p));
        }
        auto& c = w[ch];
        c.sort_objects();
        c.mark_modified();
    }
}

} // namespace

void app::populate_scene_benchmark()
{
    reset_world();
    generate_scene(M->world(), 0, 0, true);
    M->reset_fps();
}

void app::populate_scene_benchmark_walkable(uint8_t width)
{
    reset_world();
    auto& w = M->world();
    generate_scene(w, 0, 0, false);
    carve_corridor(w, 0, walk_corridor_tile, width, walk_chunk_min, walk_chunk_max);
    // Wide enough to leave a way past a baffle, narrow enough that the two sides overlap. The
    // width-1 menu entry satisfies neither and is meant to stay unroutable.
    if (width >= walk_baffle_tiles + 2 && width <= 2*walk_baffle_tiles)
        carve_corridor_baffles(w, 0, walk_corridor_tile, width, walk_chunk_min, walk_chunk_max);
    // The path and walk tests both search from wherever the player stands, and
    // ensure_player_character() spawns it at global (0,0), outside the corridor. Put it on the
    // north end, on the passable column: each side wall eats one, so that is start_tile+1.
    auto C = ensure_player_character(w);
    auto index = C->index();
    C->teleport_to(index, global_coords{chunk_coords_{0, walk_chunk_min, 0},
                                        local_coords{(uint8_t)(walk_corridor_tile+1), 2}},
                   Vector2b{}, rotation_COUNT);
    M->reset_fps();
}

void app::populate_scene_diagonal(uint8_t half_width)
{
    reset_world();
    auto& w = M->world();
    generate_scene(w, 0, 0, true);
    carve_diagonal(w, diag_u0, half_width, bench_chunk_min, bench_chunk_max);
    // The path test searches from wherever the player stands. ensure_player_character() spawns it
    // at global (0,0), which is on the band already; move it to the north-west end so the whole
    // cut is ahead of it.
    auto C = ensure_player_character(w);
    auto index = C->index();
    C->teleport_to(index, global_coords{chunk_coords_{-4, -4, 0}, local_coords{0, 0}},
                   Vector2b{}, rotation_COUNT);
    M->reset_fps();
}

// The four corner cells in cycle order, so leg k of scene_maze's tour runs corner k to corner
// k+1 and the last one closes back onto the first.
point app::maze_corner(uint32_t k)
{
    constexpr uint32_t last = maze_dim - 1;
    switch (k % 4)
    {
    case 0: return maze_cell_point(0, 0);
    case 1: return maze_cell_point(last, 0);
    case 2: return maze_cell_point(last, last);
    default: return maze_cell_point(0, last);
    }
}

void app::populate_scene_raycast_pins()
{
    reset_world();
    auto& w = M->world();
    generate_raycast_pins(w);
    // reset_world_post() already spawned it at global (0,0), which is the centre of the field.
    ensure_player_character(w);
    center_camera_on(point{});
    M->reset_fps();
}

void app::populate_scene_maze2()
{
    reset_world();
    auto& w = M->world();
    const auto m = generate_maze2();
    build_maze2(w, m.flags);
    auto C = ensure_player_character(w);
    auto index = C->index();
    C->teleport_to(index, maze2_cell_point(m.start).coord(), Vector2b{}, rotation_COUNT);
    center_camera_on(C->position());
    M->reset_fps();
}

point app::maze2_start() { return maze2_cell_point(generate_maze2().start); }
point app::maze2_goal() { return maze2_cell_point(generate_maze2().goal); }

// Three chunks each way so the centre one has all eight neighbours: a pass grid build searches
// the 3x3 neighbourhood, and a missing chunk is a different path from an empty one.
// num_pins is for the menu entry only. The driver passes none and lays them down one at a time,
// which is the whole point of that scene -- an empty chunk is not a cheap version of a full one.
void app::populate_scene_grids(uint32_t num_pins)
{
    reset_world();
    auto& w = M->world();
    auto ground = loader.ground_atlas("metal1");
    for (int16_t cy = -1; cy <= 1; cy++)
        for (int16_t cx = -1; cx <= 1; cx++)
        {
            auto& c = w[chunk_coords_{cx, cy, 0}];
            for (auto k = 0u; k < TILE_COUNT; k++)
                c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };
            c.mark_modified();
        }
    for (auto n = 0u; n < num_pins; n++)
        add_grid_pin(n);
    // add_grid_pin() fills chunk (0,0), and the world origin is that chunk's corner rather than
    // its middle. Left where reset_world_post() put it the character would watch the chunk fill
    // from one corner of it.
    auto C = ensure_player_character(w);
    auto index = C->index();
    const point centre{chunk_coords_{0, 0, 0},
                       local_coords{(uint8_t)(TILE_MAX_DIM/2), (uint8_t)(TILE_MAX_DIM/2)}, {}};
    C->teleport_to(index, centre.coord(), Vector2b{}, rotation_COUNT);
    center_camera_on(centre);
    M->reset_fps();
}

// One more pin in the centre chunk, at a hash-picked pixel. scene_grids rebuilds the grids after
// every call, so the run walks the whole occupancy curve from empty to saturated rather than
// sampling one point on it.
void app::add_grid_pin(uint32_t n)
{
    auto& w = M->world();
    const auto h = hash2(n, grid_pin_seed);
    const Vector2i at{(int)(h % chunk_size_xy) - tile_size_xy/2,
                      (int)(h / chunk_size_xy % chunk_size_xy) - tile_size_xy/2};
    const auto pt = point::normalize_coords(point{}, at);
    auto p = pin_proto(grid_pin_size);
    p.offset = pt.offset();
    w.make_scenery(w.make_id(), pt.coord(), move(p));
}

void app::populate_scene_maze()
{
    reset_world();
    auto& w = M->world();
    generate_scene(w, 0, 0, false);
    build_maze(w, generate_maze(maze_dim, maze_seed), maze_dim, bench_chunk_min, bench_chunk_max);
    auto C = ensure_player_character(w);
    auto index = C->index();
    C->teleport_to(index, maze_corner(0).coord(), Vector2b{}, rotation_COUNT);
    center_camera_on(C->position());
    M->reset_fps();
}

// The k-th light of the 3x3 testable middle. Only those nine have all 16 chunks of their lightmap
// block populated; one nearer the edge previews a quarter of empty world.
point app::lightmap_light(uint32_t k)
{
    const auto i = k % (lm_test_dim*lm_test_dim);
    const chunk_coords_ ch{(int16_t)(lm_test_min + (int)(i % lm_test_dim)),
                           (int16_t)(lm_test_min + (int)(i / lm_test_dim)), 0};
    return { ch, local_coords{lm_light_tile, lm_light_tile}, {} };
}

// The lightmap world's chunk (0,0) is the room motif -- a wall box with one doorway and four
// pillars -- which is the most legible thing in the scene to run cover rays out of.
void app::populate_scene_cover()
{
    populate_scene_lightmap();
    // The preview window opens over the cover overlay, and this scene is about the overlay.
    tested_light_chunk = {};
}

void app::populate_scene_lightmap()
{
    reset_world();
    auto& w = M->world();
    generate_lightmap_scene(w);
    auto C = ensure_player_character(w);
    auto index = C->index();
    const auto pt = lightmap_light(4);
    C->teleport_to(index, pt.coord(), Vector2b{}, rotation_COUNT);
    center_camera_on(pt);
    // What the "Lightmap test" popup item sets. Without it the menu entry builds the world and
    // leaves the preview shut, which is the one thing this scene exists to show.
    tested_light_chunk = pt.chunk3();
    M->reset_fps();
}

void app::populate_scene_benchmark_all_z()
{
    reset_world();
    generate_scene(M->world(), chunk_z_min, chunk_z_max, true);
    M->reset_fps();
}

} // namespace floormat
