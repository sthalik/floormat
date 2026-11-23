#include "torture-test.hpp"
#include "src/world.hpp"
#include "src/object-storage.inl"
#include "src/hole.hpp"
#include "src/light.hpp"
#include "src/scenery.hpp"
#include "src/wall-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/timer.hpp"
#include "compat/zip.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"

namespace floormat::torture_test {

namespace {

// Each pattern axis below uses a period from {2,3,4,5,7} on a different linear form of the
// tile coordinate, so the combined pattern repeats no sooner than every 420 tiles.

// A tile's own N/W wall strips lie *outside* it, at [T-depth, T), so only a half-extent above
// tile_size_xy/2 reaches them; anything positive reaches the S/E neighbors'. Depth here is 32.
constexpr inline Vector2ub hole_sizes[] = {
    { u8(7*tile_size_xy/4), u8(tile_size_xy/2)   }, // own W, E and S neighbors'
    { u8(tile_size_xy/2),   u8(7*tile_size_xy/4) }, // own N, E and S neighbors'
    { u8(3*tile_size_xy/2), u8(3*tile_size_xy/2) }, // all four, plus the corner piece
};

struct hole_z_profile { u8 z_offset, height; };

// A cut reaching tile_size_z lowers the wall's top cap onto the stub instead of leaving it
// at full height, so the "eats the top" cases below look different from the rest.
constexpr inline hole_z_profile hole_z_profiles[] = {
    { 0,                        u8(tile_size_z)   }, // full height, eats the top
    { 0,                        u8(tile_size_z/2) }, // wall left floating above the gap
    { u8(tile_size_z/2),   u8(tile_size_z/2) }, // eats the top
    { u8(tile_size_z/4),   u8(tile_size_z/2) }, // wall above and below
    { u8(2*tile_size_z/3), u8(tile_size_z/3) }, // eats the top, reaching it exactly
};

u32 generate_chunk(world& w, chunk_coords_ ch, const scene_assets& a, bool walls)
{
    auto& c = w[ch];
    const auto chunk_parity = pmod(ch.x + ch.y, 2);
    const auto& ground = a.ground[chunk_parity];

    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };

    u32 objects = 0;

    for (u8 ly = 0; ly < TILE_MAX_DIM; ly++)
        for (u8 lx = 0; lx < TILE_MAX_DIM; lx++)
        {
            // Shifting by a different coprime multiple of z per axis keeps stacked Z levels
            // from being copies of each other.
            const auto px = i32(ch.x)*i32(TILE_MAX_DIM) + lx + 7*ch.z;
            const auto py = i32(ch.y)*i32(TILE_MAX_DIM) + ly + 11*ch.z;
            const local_coords at{lx, ly};

            // variant -1 picks the frame by hashing the coordinate, giving per-tile variety
            // without a fourth pattern axis to keep coprime.
            if (walls)
            {
                c[at].wall_north() = { a.walls[hash2((u32)px, (u32)py) % 3], (variant_t)-1 };
                c[at].wall_west()  = { a.walls[hash2((u32)py, (u32)px) % 3], (variant_t)-1 };
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
                p.color = light_colors[hash2((u32)px ^ 0x5a5au, (u32)py) % array_size(light_colors)];
                // Tiles, not pixels: lightmap_shader::add_light() multiplies by tile_size.
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

} // namespace

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
    for (auto& [name, proto] : zip(scenery_names, a.scenery))
        proto = loader.scenery(name);
    return a;
}

void generate_scene(world& w, int z_min, int z_max, bool walls, i16 cmin, i16 cmax)
{
    const auto a = load_assets();
    auto t = Time::now();
    u32 chunks = 0, objects = 0;

    for (int z = z_min; z <= z_max; z++)
    {
        for (i16 cy = cmin; cy <= cmax; cy++)
            for (i16 cx = cmin; cx <= cmax; cx++)
            {
                objects += generate_chunk(w, {cx, cy, (i8)z}, a, walls);
                chunks++;
            }
        if (z_min != z_max)
            fm_debug("scene benchmark: z=%d done, %u chunks", z, chunks);
    }

    fm_debug("scene benchmark: %u chunks, %u objects in %.1f ms",
             chunks, objects, (double)Time::to_milliseconds(t.update()));
}

} // namespace floormat::torture_test
