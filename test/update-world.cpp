#include "app.hpp"
#include "run.hpp"
#include "src/update-world.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/nanosecond.inl"

namespace floormat::Test {

namespace {

constexpr auto chunk_a = chunk_coords_{0, 0, 0}, chunk_b = chunk_coords_{1, 0, 0};
constexpr auto start_near_east_edge = point{chunk_a, {15, 8}, {20, 0}};
constexpr auto dt_crossing_into_chunk_b = 500*Millisecond;

world make_two_chunk_world()
{
    auto w = world();
    (void)w[chunk_a];
    (void)w[chunk_b];
    return w;
}

bptr<critter> make_critter(world& w, point pos)
{
    auto proto = Run::make_proto(1.f);
    proto.playable = false;
    proto.offset = pos.offset();
    return w.make_object<critter>(w.make_id(), pos.coord(), move(proto));
}

bptr<critter> make_critter_walking_east(world& w, point pos)
{
    auto npc = make_critter(w, pos);
    npc->playable = true;
    npc->set_keys(false, true, false, true);
    return npc;
}

void test_frame_no_and_game_time()
{
    auto w = make_two_chunk_world();
    const chunk_coords_ chunks[] = { chunk_a };
    for (auto dt : { 17*Millisecond, 31*Millisecond })
    {
        const auto frame_no = w.frame_no();
        const auto game_time = w.game_time();
        update_world(w, chunks, dt);
        fm_assert_equal(frame_no + 1, w.frame_no());
        fm_assert(w.game_time() == game_time + dt);
    }
}

void test_unlisted_chunk_not_updated()
{
    auto w = make_two_chunk_world();
    const bptr<critter> listed[] = {
        make_critter(w, point{chunk_a, {3, 5}, {}}), make_critter(w, point{chunk_a, {7, 11}, {}}),
    };
    const bptr<critter> unlisted[] = {
        make_critter(w, point{chunk_b, {2, 13}, {}}), make_critter(w, point{chunk_b, {11, 7}, {}}),
    };
    const auto old_frame_no = unlisted[0]->last_frame_no;
    w.init_scripts();
    const chunk_coords_ chunks[] = { chunk_a };
    const auto frame_no = w.frame_no();
    update_world(w, chunks, 17*Millisecond);
    for (const auto& npc : listed)
        fm_assert_equal(frame_no, npc->last_frame_no);
    for (const auto& npc : unlisted)
        fm_assert_equal(old_frame_no, npc->last_frame_no);
    w.finish_scripts();
}

void test_coord_without_chunk_skipped()
{
    constexpr auto coord_without_chunk = chunk_coords_{-3, 2, 0};
    auto w = make_two_chunk_world();
    const auto in_a = make_critter(w, point{chunk_a, {3, 5}, {}});
    const auto in_b = make_critter(w, point{chunk_b, {2, 13}, {}});
    fm_assert(!w.at(coord_without_chunk));
    w.init_scripts();
    const chunk_coords_ chunks[] = { chunk_a, coord_without_chunk, chunk_b };
    const auto frame_no = w.frame_no();
    update_world(w, chunks, 17*Millisecond);
    fm_assert(!w.at(coord_without_chunk));
    fm_assert_equal(frame_no, in_a->last_frame_no);
    fm_assert_equal(frame_no, in_b->last_frame_no);
    w.finish_scripts();
}

void test_crossing_critter_updated_once()
{
    auto w = make_two_chunk_world();
    const auto npc = make_critter_walking_east(w, start_near_east_edge);
    w.init_scripts();
    const chunk_coords_ chunks[] = { chunk_a, chunk_b };
    const auto frame_no = w.frame_no();
    update_world(w, chunks, dt_crossing_into_chunk_b);
    fm_assert(npc->coord.chunk3() == chunk_b);
    fm_assert_equal(frame_no, npc->last_frame_no);

    auto w2 = make_two_chunk_world();
    const auto direct = make_critter_walking_east(w2, start_near_east_edge);
    w2.init_scripts();
    auto i = direct->index();
    direct->update(direct, i, dt_crossing_into_chunk_b);
    fm_assert(direct->coord == npc->coord);
    fm_assert(direct->offset == npc->offset);

    w2.finish_scripts();
    w.finish_scripts();
}

void test_next_critter_updated_after_one_leaves()
{
    auto w = make_two_chunk_world();
    const auto leaving = make_critter_walking_east(w, start_near_east_edge);
    const auto staying = make_critter(w, point{chunk_a, {3, 5}, {}});
    fm_assert(leaving->index() == 0 && staying->index() == 1);
    w.init_scripts();
    const chunk_coords_ chunks[] = { chunk_a, chunk_b };
    const auto frame_no = w.frame_no();
    update_world(w, chunks, dt_crossing_into_chunk_b);
    fm_assert(leaving->coord.chunk3() == chunk_b);
    fm_assert(staying->coord.chunk3() == chunk_a);
    fm_assert_equal(frame_no, leaving->last_frame_no);
    fm_assert_equal(frame_no, staying->last_frame_no);
    w.finish_scripts();
}

} // namespace

void test_update_world()
{
    test_frame_no_and_game_time();
    test_unlisted_chunk_not_updated();
    test_coord_without_chunk_skipped();
    test_crossing_critter_updated_once();
    test_next_critter_updated_after_one_leaves();
}

} // namespace floormat::Test
