#include "app.hpp"
#include "compat/cpu-topology.hpp"
#include "compat/stl/thread.hpp"
#include <bit>
#include <initializer_list>
#include <cr/StringView.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined __linux__
#include <sched.h>
#endif

namespace floormat::Test {

namespace {

constexpr u64 all_cpus = ~u64{0};

struct l3_cache_layout
{
    u32 num_cores, logical_cpus_per_core, size_bytes;
    u8 efficiency_class;
};

struct cpu_layout
{
    inplace_array<cpu_core, 64> cores;
    inplace_array<l3_cache, 64> l3_caches;
};

// Numbers the logical processors in order, cache by cache and core by core.
cpu_layout make_cpu_layout(std::initializer_list<l3_cache_layout> caches)
{
    cpu_layout l;
    u32 next_cpu = 0;
    for (const auto& c : caches)
    {
        u64 cache_mask = 0;
        for (u32 i = 0; i < c.num_cores; i++)
        {
            const u64 core_mask = ((u64{1} << c.logical_cpus_per_core) - 1) << next_cpu;
            next_cpu += c.logical_cpus_per_core;
            l.cores.push_back({core_mask, c.efficiency_class});
            cache_mask |= core_mask;
        }
        l.l3_caches.push_back({cache_mask, c.size_bytes});
    }
    return l;
}

inplace_array<u64, 64> consecutive_core_masks(u32 first_cpu, u32 num_cores, u32 cpus_per_core)
{
    inplace_array<u64, 64> masks;
    for (u32 i = 0; i < num_cores; i++)
        masks.push_back(((u64{1} << cpus_per_core) - 1) << (first_cpu + i * cpus_per_core));
    return masks;
}

thread_pool_cpus pick_cpus(const cpu_layout& l, u64 affinity_mask = all_cpus)
{
    return pick_thread_pool_cpus(l.cores, l.l3_caches, affinity_mask);
}

void check_picked_cpus(const thread_pool_cpus& r, ArrayView<const u64> core_masks, bool is_smt_used, u32 num_workers)
{
    fm_assert(r.core_masks.size() == core_masks.size());
    u64 logical_cpu_mask = 0;
    for (u32 i = 0; i < core_masks.size(); i++)
    {
        fm_assert(r.core_masks[i] == core_masks[i]);
        logical_cpu_mask |= core_masks[i];
    }
    fm_assert(r.logical_cpu_mask == logical_cpu_mask);
    fm_assert(r.is_smt_used == is_smt_used);
    fm_assert(r.num_workers == num_workers);
}

void test_pick_thread_pool_cpus()
{
    constexpr u32 MiB = 1 << 20;
    const auto ryzen_9950x3d = make_cpu_layout({{8, 2, 96 * MiB, 0}, {8, 2, 32 * MiB, 0}});

    check_picked_cpus(pick_cpus(ryzen_9950x3d), consecutive_core_masks(0, 8, 2), false, 7);
    fm_assert(pick_cpus(ryzen_9950x3d).logical_cpu_mask == 0xFFFF);
    check_picked_cpus(pick_cpus(make_cpu_layout({{8, 2, 32 * MiB, 0}, {8, 2, 96 * MiB, 0}})),
                      consecutive_core_masks(16, 8, 2), false, 7);
    check_picked_cpus(pick_cpus(make_cpu_layout({{8, 2, 96 * MiB, 0}})), consecutive_core_masks(0, 8, 2), false, 7);
    check_picked_cpus(pick_cpus(make_cpu_layout({{6, 2, 96 * MiB, 0}, {6, 2, 32 * MiB, 0}})),
                      consecutive_core_masks(0, 6, 2), true, 10);
    check_picked_cpus(pick_cpus(make_cpu_layout({{6, 2, 96 * MiB, 0}})), consecutive_core_masks(0, 6, 2), true, 10);
    check_picked_cpus(pick_cpus(make_cpu_layout({{8, 2, 32 * MiB, 0}, {8, 2, 32 * MiB, 0}})),
                      consecutive_core_masks(0, 8, 2), false, 7);
    check_picked_cpus(pick_cpus(make_cpu_layout({{2, 2, 8 * MiB, 0}, {2, 2, 8 * MiB, 0}})),
                      consecutive_core_masks(0, 4, 2), true, 6);
    check_picked_cpus(pick_cpus(make_cpu_layout({{4, 2, 16 * MiB, 0}, {4, 2, 16 * MiB, 0}})),
                      consecutive_core_masks(0, 8, 2), false, 7);
    check_picked_cpus(pick_cpus(make_cpu_layout({{3, 2, 16 * MiB, 0}, {3, 2, 16 * MiB, 0},
                                            {3, 2, 16 * MiB, 0}, {3, 2, 16 * MiB, 0}})),
                      consecutive_core_masks(0, 6, 2), true, 10);
    check_picked_cpus(pick_cpus(make_cpu_layout({{4, 2, 16 * MiB, 0}, {8, 2, 8 * MiB, 0}})),
                      consecutive_core_masks(0, 12, 2), false, 11);
    check_picked_cpus(pick_cpus(make_cpu_layout({{4, 2, 16 * MiB, 1}, {8, 2, 8 * MiB, 0}})),
                      consecutive_core_masks(0, 4, 2), true, 6);

    // The i7-14700K's 12 efficiency cores, of one logical processor each, share the performance cores' cache.
    auto i7_14700k = make_cpu_layout({{8, 2, 33 * MiB, 1}});
    for (u32 i = 0; i < 12; i++)
    {
        i7_14700k.cores.push_back({u64{1} << (16 + i), 0});
        i7_14700k.l3_caches[0].logical_cpu_mask |= u64{1} << (16 + i);
    }
    check_picked_cpus(pick_cpus(i7_14700k), consecutive_core_masks(0, 8, 2), false, 7);

    check_picked_cpus(pick_cpus(make_cpu_layout({{6, 1, 32 * MiB, 0}})), consecutive_core_masks(0, 6, 1), true, 5);

    const u64 cpus_6_and_7[] = {0xC0};
    check_picked_cpus(pick_cpus(ryzen_9950x3d, 0xC0), cpus_6_and_7, true, 0);

    inplace_array<u64, 64> first_cpu_of_each_core;
    for (u32 i = 0; i < 8; i++)
        first_cpu_of_each_core.push_back(u64{1} << 2*i);
    check_picked_cpus(pick_cpus(ryzen_9950x3d, 0x55555555), first_cpu_of_each_core, false, 7);

    check_picked_cpus(pick_thread_pool_cpus({}, {}, 0xFF), consecutive_core_masks(0, 8, 1), false, 7);
    check_picked_cpus(pick_thread_pool_cpus({}, {}, 0x0F), consecutive_core_masks(0, 4, 1), true, 3);

    check_picked_cpus(pick_cpus(ryzen_9950x3d, 0), {}, false, 0);
}

void test_parse_cpu_list()
{
    fm_assert(parse_cpu_list("0"_s) == 0x1);
    fm_assert(parse_cpu_list("0-3"_s) == 0xF);
    fm_assert(parse_cpu_list("0-3,8-11"_s) == 0xF0F);
    fm_assert(parse_cpu_list("0,2,4"_s) == 0x15);
    fm_assert(parse_cpu_list("5-7,13\n"_s) == 0x20E0);
    fm_assert(parse_cpu_list("62-65"_s) == 0xC000000000000000);
    fm_assert(parse_cpu_list(""_s) == 0);
}

void test_query_thread_pool_cpus()
{
    const thread_pool_cpus r = query_thread_pool_cpus();
    fm_assert(!r.core_masks.empty());
    u64 logical_cpu_mask = 0;
    for (u32 i = 0; i < r.core_masks.size(); i++)
    {
        const u64 m = r.core_masks[i];
        fm_assert(m != 0);
        fm_assert((m & logical_cpu_mask) == 0);
        fm_assert(i == 0 || std::countr_zero(r.core_masks[i - 1]) < std::countr_zero(m));
        logical_cpu_mask |= m;
    }
    fm_assert(r.logical_cpu_mask == logical_cpu_mask);
    fm_assert((r.logical_cpu_mask & ~process_affinity_mask()) == 0);
    fm_assert(r.num_workers < (u32)std::popcount(r.logical_cpu_mask));
}

void test_set_current_thread_affinity()
{
    const u64 mask = query_thread_pool_cpus().core_masks[0];
    bool is_affinity_set = false;
    u32 cpu = (u32)-1;
    std::thread worker{[&] {
        is_affinity_set = set_current_thread_affinity(mask);
#ifdef _WIN32
        cpu = GetCurrentProcessorNumber();
#elif defined __linux__
        cpu = (u32)sched_getcpu();
#endif
    }};
    worker.join();
#if defined _WIN32 || defined __linux__
    fm_assert(is_affinity_set);
    fm_assert(cpu < 64 && (mask >> cpu & 1));
#else
    (void)cpu;
    fm_assert(!is_affinity_set);
#endif
}

} // namespace

void test_cpu_topology()
{
    test_pick_thread_pool_cpus();
    test_parse_cpu_list();
    test_query_thread_pool_cpus();
    test_set_current_thread_affinity();
}

} // namespace floormat::Test
