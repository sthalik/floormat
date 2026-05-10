#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "src/tile-defs.hpp"
#include "src/tile-constants.hpp"
#include "src/global-coords.hpp"
#include "src/nanosecond.inl"
#include "src/point.inl"
#include "test/run.hpp"

namespace floormat {
namespace {

constexpr float TS = (float)tile_size_xy;
constexpr float CHUNK_EXTENT = TS * (float)TILE_MAX_DIM;
constexpr Ns dt_60hz = Second / 60;

void place_pillar(world& w, chunk_coords_ ch, local_coords tile, Vector2b bb_off, Vector2ub bb_size)
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_offset = bb_off;
    p.bbox_size   = bb_size;
    p.pass        = pass_mode::blocked;
    w.make_scenery(w.make_id(), {ch, tile}, move(p));
}

void fill_ground(chunk& c)
{
    auto ground = loader.ground_atlas("floor-tiles");
    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };
}

float pos_to_origin_chunk_local_x(point p)
{
    return (float)p.chunk3().x * CHUNK_EXTENT + (float)p.local().x * TS + (float)p.offset().x();
}

void test_critter_slit_ne()
{
    // pillar geometry from editor/world.cpp:populate_sweep_aabb_slit.
    auto w = world();
    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];
    fill_ground(c);
    place_pillar(w, ch, {0, 0}, Vector2b{ 0, -1}, Vector2ub{2, 2});
    place_pillar(w, ch, {1, 1}, Vector2b{ 1,  0}, Vector2ub{2, 2});

    object_id id = 0;
    auto proto = Run::make_proto(20.f);
    proto.bbox_size = Vector2ub(tile_size_xy);
    auto C = w.ensure_player_character(id, proto);
    {
        auto i = C->index();
        C->teleport_to(i, global_coords{ch, {1, 1}}, Vector2b{-32, -32}, rotation_COUNT);
    }
    c.mark_modified();
    w.init_scripts();

    const auto initial = C->position();

    // arrows_to_dir is iso-screen: R-only → world NE, L-only → world SW.
    C->set_keys(false, true, false, false);
    for (int k = 0; k < 60 * 2; k++)
    {
        auto idx = C->index();
        C->update(C, idx, dt_60hz);
    }
    const auto after_ne = C->position();
    const auto dist_ne = point::distance_l1(after_ne, initial);
    fm_assert(dist_ne == 0);

    C->set_keys(true, false, false, false);
    for (int k = 0; k < 60 * 2; k++)
    {
        auto idx = C->index();
        C->update(C, idx, dt_60hz);
    }
    const auto after_sw = C->position();
    const auto dist_sw = point::distance_l1(after_sw, after_ne);
    fm_assert(dist_sw > 16);

    w.finish_scripts();
}

} // namespace

void Test::test_sweep_aabb()
{
    test_critter_slit_ne();
}

} // namespace floormat
