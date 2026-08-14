#include "app.hpp"
#include "src/world.hpp"
#include "src/hole.hpp"
#include "src/light.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/wall-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "src/timer.hpp"
#include "src/nanosecond.hpp"
#include "compat/array-size.hpp"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"
#include "floormat/main.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include <array>
#include <mg/Vector4.h>

namespace floormat {

namespace {

constexpr inline int16_t bench_chunk_min = -8, bench_chunk_max = 8;

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

uint32_t generate_chunk(world& w, chunk_coords_ ch, const scene_assets& a)
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
            c[at].wall_north() = { a.walls[hash2((uint32_t)px, (uint32_t)py) % 3], (variant_t)-1 };
            c[at].wall_west()  = { a.walls[hash2((uint32_t)py, (uint32_t)px) % 3], (variant_t)-1 };

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
                p.max_distance = tile_size_xy * 3; // lightmap skips a light under 1e-6
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

void generate_scene(world& w, int z_min, int z_max)
{
    const auto a = load_assets();
    auto t = Time::now();
    uint32_t chunks = 0, objects = 0;

    for (int z = z_min; z <= z_max; z++)
    {
        for (int16_t cy = bench_chunk_min; cy <= bench_chunk_max; cy++)
            for (int16_t cx = bench_chunk_min; cx <= bench_chunk_max; cx++)
            {
                objects += generate_chunk(w, {cx, cy, (int8_t)z}, a);
                chunks++;
            }
        if (z_min != z_max)
            fm_debug("scene benchmark: z=%d done, %u chunks", z, chunks);
    }

    fm_debug("scene benchmark: %u chunks, %u objects in %.1f ms",
             chunks, objects, (double)Time::to_milliseconds(t.update()));
}

} // namespace

void app::populate_scene_benchmark()
{
    reset_world();
    generate_scene(M->world(), 0, 0);
    M->reset_fps();
}

void app::populate_scene_benchmark_all_z()
{
    reset_world();
    generate_scene(M->world(), chunk_z_min, chunk_z_max);
    M->reset_fps();
}

} // namespace floormat
