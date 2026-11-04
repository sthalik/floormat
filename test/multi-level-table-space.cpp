#include "app.hpp"
#include "multi-level-table-common.hpp"
#include "compat/borrowed-ptr.inl"
#include "src/chunk-table.hpp"
#include "src/object-table.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/light.hpp"
#include "src/global-coords.hpp"
#include "src/tile-defs.hpp"

namespace floormat {

using namespace floormat::mlt_test;

namespace {

using chunk_table_mlt = multi_level_table<chunk*, detail::chunk_table_params>;

static_assert(move_only<chunk_table_mlt>);
static_assert(top_bytes<chunk_table_mlt> == 2 << 20 && sizeof(chunk*) << chunk_table_mlt::page_bits == 2 << 20);

static_assert(chunk_table_mlt::dims == 3 && chunk_table_mlt::key_bits == 36);
static_assert(packs_to<chunk_table_mlt>({1, 0, 0}, 1) && packs_to<chunk_table_mlt>({0, 1, 0}, 1 << 9));
static_assert(packs_to<chunk_table_mlt>({1 << 9, 0, 0}, u64{1} << 22) && packs_to<chunk_table_mlt>({0, 1 << 9, 0}, u64{1} << 29));
static_assert(packs_to<chunk_table_mlt>({0, 0, 1}, 1 << 18) && packs_to<chunk_table_mlt>({0xffff, 0xffff, 15}, (u64{1} << 36) - 1));

static_assert(space_deep_table::dims == 3 && space_deep_table::key_bits == 12);
static_assert(packs_to<space_deep_table>({1, 0, 0}, 1) && packs_to<space_deep_table>({0, 1, 0}, 8) && packs_to<space_deep_table>({0, 0, 1}, 64));
static_assert(packs_to<space_deep_table>({8, 0, 0}, 256) && packs_to<space_deep_table>({0, 8, 0}, 1024) && packs_to<space_deep_table>({31, 31, 3}, 4095));
static_assert(packs_to<space_free_table>({0, 1, 0}, 4) && packs_to<space_free_table>({4, 0, 0}, 8) && packs_to<space_free_table>({0, 2, 0}, 16));
static_assert(packs_to<space_free_table>({0, 0, 1}, 32));
static_assert(packs_to<space_zero_free_table>({0, 0, 1}, 16) && packs_to<space_zero_free_table>({0, 0, 2}, 32) && packs_to<space_zero_free_table>({4, 4, 0}, 192));
static_assert(packs_to<space_one_entry_table>({1, 0, 0}, 1) && packs_to<space_one_entry_table>({0, 0, 1}, 2));
static_assert(packs_to<space_large_table>({0, 0, 1}, 16) && packs_to<space_large_table>({4, 0, 0}, 64) && packs_to<space_large_table>({0, 4, 0}, 128));
static_assert(packs_to<space_large_table>({8, 0, 0}, 256) && packs_to<space_large_table>({0, 8, 0}, 512) && packs_to<space_large_table>({0, 0, 4}, 1024));
static_assert(packs_to<space_large_table>({15, 15, 7}, 2047) && top_bytes<space_large_table> == 24 << 3);

void check_chunk_table()
{
    const i16 coords[] = { INT16_MIN, INT16_MAX, -257, -256, 255, 256, };
    const i8 zs[] = { chunk_z_min, chunk_z_max, };
    const auto fake = [](u32 i) { return reinterpret_cast<chunk*>(uptr{64} * (i + 1)); };

    detail::chunk_table t;
    u32 i = 0;
    for (i8 z : zs)
        for (i16 v : coords)
            t.update_slot({v, v, z}, fake(i++));
    i = 0;
    for (i8 z : zs)
        for (i16 v : coords)
        {
            fm_assert(t.chunk_at({v, v, z}) == fake(i++));
            fm_assert(!t.chunk_at({i16(v + 1), v, z}) && !t.chunk_at({i16(v - 1), v, z}));
            fm_assert(!t.chunk_at({v, i16(v + 1), z}) && !t.chunk_at({v, i16(v - 1), z}));
        }

    // chunk_coords_ addition wraps
    t.update_slot({INT16_MIN, INT16_MAX, chunk_z_max}, fake(99));
    bool found = false;
    for (chunk* c : t.neighbors({INT16_MAX, INT16_MAX, chunk_z_max}))
        if (c == fake(99))
            found = true;
    fm_assert(found);
    t.update_slot({INT16_MIN, INT16_MAX, chunk_z_max}, nullptr);

    for (i8 z : zs)
        for (i16 v : coords)
        {
            t.update_slot({v, v, z}, nullptr);
            fm_assert(!t.chunk_at({v, v, z}));
        }
}

void check_world_ids()
{
    constexpr object_id max_id = (object_id{1} << object_table::key_bits) - 1;
    const object_id ids[] = { 1025, (1 << 18) - 1, 1 << 18, };
    constexpr chunk_coords_ ch{0, 0, 0};

    world w;
    u32 i = 0;
    for (object_id id : ids)
        (void)w.make_object<light>(id, {ch, local_coords{i++}}, light_proto{});
    w.set_object_counter(max_id - 1);
    const auto id = w.make_id();
    fm_assert(id == max_id);
    (void)w.make_object<light>(id, {ch, local_coords{i++}}, light_proto{});

    for (object_id x : ids)
    {
        const auto o = w.find_object(x);
        fm_assert(o && o->id == x);
    }
    fm_assert(w.find_object(max_id)->id == max_id);
    w.chunk_table_prepare_frame();

    {
        const auto o = w.find_object(ids[1]);
        o->chunk().kill_object(*o, o->index());
    }
    fm_assert(!w.find_object(ids[1]));
    w.chunk_table_prepare_frame();
}

void check_world_random_ids()
{
    constexpr object_id max_id = (object_id{1} << object_table::key_bits) - 1;
    constexpr chunk_coords_ ch{0, 0, 0};
    constexpr u32 count = 64;

    Random::xoshiro256starstar rng;
    Random::seed(rng, 17);
    const object_id cluster = 1025 + Random::next(rng) % (max_id - 1024 - 4096);
    world w;
    object_id ids[count];
    bool dead[count] = {};
    gtl::flat_hash_set<object_id> seen;
    for (u32 i = 0; i < count; )
    {
        const object_id id = i % 2 ? 1025 + Random::next(rng) % (max_id - 1024) : cluster + Random::next(rng) % 4096;
        if (!seen.insert(id).second)
            continue;
        ids[i] = id;
        (void)w.make_object<light>(id, {ch, local_coords{i}}, light_proto{});
        i++;
    }
    for (u32 i = 0; i < count; i++)
        if (Random::next(rng) % 2)
        {
            const auto o = w.find_object(ids[i]);
            o->chunk().kill_object(*o, o->index());
            dead[i] = true;
        }
    w.chunk_table_prepare_frame();
    for (u32 i = 0; i < count; i++)
    {
        const auto o = w.find_object(ids[i]);
        if (dead[i])
            fm_assert(!o);
        else
            fm_assert(o && o->id == ids[i]);
    }
    for (u32 i = 0; i < 256; i++)
        if (const object_id id = 1 + Random::next(rng) % max_id; !seen.contains(id))
            fm_assert(!w.find_object(id));
}

constexpr u64 pack_coords(chunk_coords_ c)
{
    return u64{u16(c.x)} | u64{u16(c.y)} << 16 | u64{u8(c.z)} << 32;
}

constexpr chunk_coords_ unpack_coords(u64 k)
{
    return { i16(u16(k)), i16(u16(k >> 16)), i8(u8(k >> 32)) };
}

chunk* fake_chunk(u32 i)
{
    return reinterpret_cast<chunk*>(uptr{64} * (i + 1));
}

// The key layout, derived apart from make_key() so the test doesn't compare it with itself.
constexpr u32 chunk_bias = (1u << 15) + (1u << 8);

constexpr i16 chunk_coord(u32 outer, u32 side)
{
    return i16(u16((outer << 9 | side) - chunk_bias));
}

constexpr u32 chunk_top_index(u32 oy, u32 ox, i8 z)
{
    return oy << 11 | ox << 4 | u32(z - chunk_z_min);
}

constexpr u32 chunk_slot(u32 ly, u32 lx)
{
    return ly << 9 | lx;
}

void check_chunk_table_layout()
{
    detail::chunk_table t;
    const auto& mlt = t.raw_table();
    t.update_slot({0, 0, 0}, fake_chunk(0));
    fm_assert(mlt.page_count() == 1);
    fm_assert(mlt.raw_pages()[0].top_index == chunk_top_index(64, 64, 0));
    chunk* const* page = mlt.raw_pages()[0].page;
    fm_assert(page[chunk_slot(256, 256)] == fake_chunk(0));

    t.update_slot({-256, 0, 0}, fake_chunk(1));
    t.update_slot({255, 0, 0}, fake_chunk(2));
    t.update_slot({0, -256, 0}, fake_chunk(3));
    t.update_slot({0, 255, 0}, fake_chunk(4));
    fm_assert(mlt.page_count() == 1);
    fm_assert(page[chunk_slot(256, 0)] == fake_chunk(1) && page[chunk_slot(256, 511)] == fake_chunk(2));
    fm_assert(page[chunk_slot(0, 256)] == fake_chunk(3) && page[chunk_slot(511, 256)] == fake_chunk(4));

    t.update_slot({-257, 0, 0}, fake_chunk(5));
    t.update_slot({256, 0, 0}, fake_chunk(6));
    t.update_slot({0, -257, 0}, fake_chunk(7));
    t.update_slot({0, 256, 0}, fake_chunk(8));
    t.update_slot({0, 0, 1}, fake_chunk(9));
    fm_assert(mlt.page_count() == 6);
    const u32 tops[] = {
        chunk_top_index(64, 64, 0), chunk_top_index(64, 63, 0), chunk_top_index(64, 65, 0),
        chunk_top_index(63, 64, 0), chunk_top_index(65, 64, 0), chunk_top_index(64, 64, 1),
    };
    for (u32 top : tops)
    {
        u32 found = 0;
        for (const auto& rec : mlt.raw_pages())
            found += rec.top_index == top;
        fm_assert(found == 1);
    }

    t = {};
    t.update_slot({INT16_MIN, 0, chunk_z_max}, fake_chunk(10));
    t.update_slot({INT16_MAX, 0, chunk_z_max}, fake_chunk(11));
    fm_assert(mlt.page_count() == 1);
    fm_assert(mlt.raw_pages()[0].top_index == chunk_top_index(64, 0, chunk_z_max));
    page = mlt.raw_pages()[0].page;
    fm_assert(page[chunk_slot(256, 256)] == fake_chunk(10) && page[chunk_slot(256, 255)] == fake_chunk(11));
}

void check_chunk_table_random()
{
    Random::xoshiro256starstar rng;
    Random::seed(rng, 18);
    gtl::flat_hash_map<u64, chunk*> ref;
    detail::chunk_table t;
    const auto& mlt = t.raw_table();
    u32 serial = 0;

    const auto lookup = [&](chunk_coords_ c) -> chunk* {
        const auto it = ref.find(pack_coords(c));
        return it != ref.end() ? it->second : nullptr;
    };
    const auto random_z = [&] { return i8(chunk_z_min + i32(Random::next(rng) % u32(chunk_z_count))); };
    const auto random_coords = [&] {
        const u64 r = Random::next(rng);
        return chunk_coords_{i16(r), i16(r >> 16), random_z()};
    };
    const auto near_corner = [&] {
        const u64 r = Random::next(rng);
        const auto x = i16(chunk_coord(u32(r & 127), 0) - 8 + i32(r >> 8 & 15));
        const auto y = i16(chunk_coord(u32(r >> 12 & 127), 0) - 8 + i32(r >> 20 & 15));
        return chunk_coords_{x, y, random_z()};
    };
    const auto check_neighbors = [&](chunk_coords_ c) {
        const auto nb = t.neighbors(c);
        for (u32 i = 0; i < 8; i++)
            fm_assert(nb[i] == lookup(c + world::neighbor_offsets[i]));
    };
    const auto check = [&] {
        u32 n = 0;
        for (const auto& rec : mlt.raw_pages())
        {
            fm_assert(!rec.zero_index);
            const u32 oy = rec.top_index >> 11, ox = rec.top_index >> 4 & 127;
            const auto z = i8(i32(rec.top_index & 15) + chunk_z_min);
            for (u32 i = 0; i < 1u << 18; i++)
                if (chunk* c = rec.page[i])
                {
                    fm_assert(lookup({chunk_coord(ox, i & 511), chunk_coord(oy, i >> 9), z}) == c);
                    n++;
                }
        }
        fm_assert(n == ref.size());
        for (const auto& [k, c] : ref)
        {
            fm_assert(t.chunk_at(unpack_coords(k)) == c);
            check_neighbors(unpack_coords(k));
        }
        for (u32 i = 0; i < 64; i++)
            check_neighbors(i % 2 ? random_coords() : near_corner());
    };

    chunk_coords_ pos{0, 0, 0};
    u32 resets = 0;
    for (u32 op = 1; op <= 8000; op++)
    {
        const u64 r = Random::next(rng);
        if (op / 1000 % 4 == 3)
            pos = random_coords();
        else if (r % 256 == 0)
            pos = r >> 8 & 1 ? near_corner() : random_coords();
        else
        {
            pos = pos + world::neighbor_offsets[r >> 9 & 7];
            if ((r >> 12) % 32 == 0)
                pos = {pos.x, pos.y, i8(chunk_z_min + (pos.z - chunk_z_min + (r >> 17 & 1 ? 1 : chunk_z_count - 1)) % chunk_z_count)};
        }
        const bool ins = (r >> 20) % 5 < (op / 500 % 2 ? 1u : 4u);
        if (ins && !lookup(pos))
        {
            chunk* c = fake_chunk(serial++);
            t.update_slot(pos, c);
            ref[pack_coords(pos)] = c;
        }
        else if (!ins)
        {
            t.update_slot(pos, nullptr);
            ref.erase(pack_coords(pos));
        }
        fm_assert(t.chunk_at(pos) == lookup(pos));
        if (mlt.page_count() >= 8)
        {
            if (resets++ % 8 == 0)
                check();
            t = {};
            ref.clear();
        }
        if (op % 1000 == 0)
            check();
    }
    check();
}

} // namespace

template class multi_level_table<tracked, space_free_params>;
template class multi_level_table<tracked, space_deep_params>;
template class multi_level_table<u32, space_deep_params>;
template class multi_level_table<tracked, space_zero_free_params>;
template class multi_level_table<tracked, space_no_pages_params>;
template class multi_level_table<tracked, space_one_entry_params>;
template class multi_level_table<tracked, space_large_params>;

void mlt_test::test_space_tables()
{
    check_pack<space_free_table>(0);
    check_pack<space_deep_table>(0);
    check_pack<space_zero_free_table>(0);
    check_pack<space_no_pages_table>(0);
    check_pack<space_one_entry_table>(0);
    check_pack<space_large_table>(0);
    check_pack<chunk_table_mlt>(90);

    check_fill<space_free_table>(70);
    check_fill<space_deep_table>(71);
    check_fill<space_deep_int_table>(72);
    check_fill<space_zero_free_table>(73);
    check_fill<space_no_pages_table>(74);
    check_fill<space_one_entry_table>(75);
    check_fill<space_large_table>(92);
    fm_assert(tracked::live == 0);

    check_random<space_free_table>(with(83, usize{16} << 3));
    check_random<space_deep_table>({.seed = 84, .ops = 12000, .top_size_bytes = usize{8} << 6, .hooks = true});
    check_random<space_deep_int_table>({.seed = 85, .ops = 12000, .top_size_bytes = usize{8} << 6});
    check_random<space_zero_free_table>(with(86, usize{24} << 2));
    check_random<space_no_pages_table>(with(87, usize{8} << 4));
    check_random<space_one_entry_table>(with(88, 8));
    fm_assert(tracked::live == 0);

    {
        using enum walk_type;
        check_walk<space_large_table>(102, simple);
        check_walk<space_deep_table>(105, levy_flight);
        check_walk<space_zero_free_table>(108, persistent);
        check_walk<space_deep_table>(111, kinetic_growth);
        check_walk<space_deep_int_table>(114, true_self_avoiding);
        check_walk<space_large_table>(117, loop_erased);
        check_walk<space_zero_free_table>(120, vertex_reinforced);
        check_walk<space_deep_table>(123, elephant);
        check_walk<space_deep_int_table>(126, resetting);
        check_walk<space_large_table>(129, random_environment);
        check_walk<space_deep_table>(132, excited);
        check_walk<space_deep_table>(135, langton_ant);
        check_walk<space_zero_free_table>(138, rotor_router);
        check_walk<space_large_table>(141, internal_dla);
        check_walk<space_zero_free_table>(144, eden);
        check_retrace<space_deep_table>(147, 8);
        check_walk<space_large_table>(150, page_corner_drift);
        check_retrace<space_large_table>(153, 8);
        check_retrace<space_deep_table>(157, 32);
    }
    fm_assert(tracked::live == 0);

    check_chunk_table();
    check_chunk_table_layout();
    check_chunk_table_random();
    check_world_ids();
    check_world_random_ids();
}

} // namespace floormat
