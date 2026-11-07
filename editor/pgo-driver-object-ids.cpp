#include "pgo-driver.hpp"
#include "app.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile.hpp"
#include "src/ground-atlas.hpp"
#include "src/light.hpp"
#include "src/point.hpp"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/enumerate.hpp"
#include "floormat/main.hpp"
#include <mg/Functions.h>

#ifndef FLOORMAT_NO_PGO_DRIVER

namespace floormat {

using pgo::task;

task app::scene_object_ids()
{
    constexpr u32 num_objects = 1u << 18;
    constexpr u32 per_chunk = TILE_COUNT;
    constexpr u32 chunks_per_row = 32;
    // Centered on the origin, where it used to sit far outside get_draw_bounds(). Only what the
    // draw bounds cover is updated and drawn -- a screenful of lights against a quarter million in
    // the table -- and watching those go out is the only sign the scene is doing anything.
    constexpr int chunk_x0 = -(int)(chunks_per_row/2), chunk_y0 = chunk_x0;
    // Ground under the middle of the farm, which is all the draw bounds ever reach. Lights are
    // virtual, so without it the sprites hang over nothing.
    constexpr i16 ground_radius = 3;
    // An eighth, not a quarter: the rounds are what the scene samples the table at, and this
    // doubles how many of them there are between full and empty.
    constexpr u32 kill_divisor = 8, short_kill_divisor = 2;
    const auto divisor = pgo_driver_use_short_scenes() ? short_kill_divisor : kill_divisor;

    auto& w = M->world();
    auto ground = loader.ground_atlas("metal1");
    for (i16 cy = -ground_radius; cy <= ground_radius; cy++)
        for (i16 cx = -ground_radius; cx <= ground_radius; cx++)
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

    for (auto& [i, id] : enumerate(live))
    {
        const auto n = i / per_chunk;
        const chunk_coords_ ch{(i16)(chunk_x0 + (int)(n % chunks_per_row)),
                               (i16)(chunk_y0 + (int)(n / chunks_per_row)), 0};
        light_proto p;
        // A zero bbox keeps the object out of the RTree, which nothing here is measuring.
        p.bbox_size = {};
        id = w.make_object<light>(w.make_id(), {ch, local_coords{i % per_chunk}}, p)->id;
        id1 = Math::max(id1, id);
    }
    fm_assert_equal((usize)num_objects, (usize)(id1 - id0 + 1));
    co_yield {};

    u64 rng = 0x9e3779b97f4a7c15u;
    for (auto i = num_objects; i-- > 1; )
    {
        rng = rng*6364136223846793005u + 1442695040888963407u;
        const auto j = (u32)((rng >> 32) % (i + 1));
        const auto tmp = live[i]; live[i] = live[j]; live[j] = tmp;
    }

    u32 num_live = num_objects, rounds = 0;

    for (;;)
    {
        u32 hits = 0;
        for (auto id = id0; id <= id1; id++)
            if (w.find_object(id))
                hits++;
        fm_assert_equal(num_live, hits);
        rounds++;
        co_yield {};

        if (num_live == 0)
            break;
        // An eighth of seven is none, and the loop would never end.
        const auto num_killed = Math::max(num_live/divisor, 1u);
        for (auto i = num_live - num_killed; i < num_live; i++)
        {
            auto o = w.find_object(live[i]);
            fm_assert(o);
            // kill_object() deletes the object out from under this bptr on purpose: the chunk
            // owns the lifetime, and a borrowed pointer observes the death instead of delaying it.
            o->chunk().kill_object(*o, o->index());
        }
        num_live -= num_killed;
    }

    fm_debug("object_ids: %u objects, ids %zu..%zu, %u rounds, %zu lookups", num_objects,
             (usize)id0, (usize)id1, rounds, (usize)rounds * num_objects);
}

} // namespace floormat

#endif
