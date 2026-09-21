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
#include "src/tile-defs.hpp"
#include "src/tile-constants.hpp"
#include "src/global-coords.hpp"
#include "src/nanosecond.inl"
#include "src/point.inl"

namespace floormat {
namespace {

constexpr Ns dt_60hz = Second / 60;
constexpr chunk_coords_ ch{0, 0, 0};

// Walls can't stand in for the pillars: a wall collider is a strip of wall_atlas depth outside the
// tile, so it eats into the gap and no tile-sized bbox fits at any offset.
constexpr uint8_t pillar_x0 = 6, free_x = 7, pillar_x1 = 8;
constexpr uint8_t start_y = 2;
constexpr float speed = 10;
// 10 px per update() at this speed, so 80 stops short of the last pillar row.
constexpr uint32_t frames = 80;
constexpr int expect_px = 10 * tile_size_xy;

void pillar(world& w, local_coords tile)
{
    scenery_proto p;
    p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype   = generic_scenery_proto{};
    p.bbox_size = Vector2ub(tile_size_xy);
    p.pass      = pass_mode::blocked;
    w.make_scenery(w.make_id(), {ch, tile}, move(p));
}

bptr<critter> add_player(world& w)
{
    object_id id = 0;
    auto proto = Run::make_proto(speed);
    // Run::make_proto() halves it; the tile-sized default from object.hpp is the subject here.
    proto.bbox_size = Vector2ub(tile_size_xy);
    auto C = w.ensure_player_character(id, move(proto));
    Run::mark_all_modified(w);
    w.init_scripts();
    return C;
}

// The gap admits the bbox at exactly one x. The walk clears it only because rect_intersects and
// axis_overlap both treat edge contact as no contact.
void test_corridor_south()
{
    auto w = world();
    for (uint8_t y = 0; y < TILE_MAX_DIM; y++)
    {
        pillar(w, {pillar_x0, y});
        pillar(w, {pillar_x1, y});
    }

    auto C = add_player(w);
    {
        auto i = C->index();
        C->teleport_to(i, global_coords{ch, {free_x, start_y}}, Vector2b{}, rotation_COUNT);
        C->delta = 0;
        C->offset_frac = 0;
    }

    const auto initial = C->position();
    C->set_keys(true, false, false, true); // world S; see test/slide.cpp:27
    for (auto n = 0u; n < frames; n++)
    {
        auto i = C->index();
        C->update(C, i, dt_60hz);
    }
    C->set_keys(false, false, false, false);

    if (const auto d = C->position() - initial; d.x() != 0 || d.y() < expect_px)
    {
        Error{standard_error()} << "!!! fatal: corridor walk moved by" << d;
        fm_assert(false);
    }
    w.finish_scripts();
}

} // namespace

void Test::test_corridor()
{
    test_corridor_south();
}

} // namespace floormat
