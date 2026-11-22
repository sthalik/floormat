#pragma once
#include "inplace-array.hpp"

namespace floormat {

// Bit i of logical_cpu_mask is logical processor i. A core with a higher efficiency_class is faster.
// Where the OS reports no classes, every core has 0.
struct cpu_core { u64 logical_cpu_mask; u8 efficiency_class; };

// The logical processors sharing one level-3 cache. size_bytes is 0 when the OS doesn't report it.
struct l3_cache { u64 logical_cpu_mask; u32 size_bytes; };

// core_masks holds one mask per picked core, ordered by lowest bit, and logical_cpu_mask is their OR.
// The main thread gets core_masks[0], and the num_workers workers get the rest of logical_cpu_mask.
struct thread_pool_cpus
{
    inplace_array<u64, 64> core_masks;
    u64 logical_cpu_mask;
    u32 num_workers;
    bool is_smt_used; // True when workers may run on every logical processor of a core.
};

// Caches are added while fewer cores than this are picked. The smallest V-Cache die has 6 cores.
constexpr inline u32 min_pool_cores = 6;
// Below this many picked cores, workers also run on SMT siblings, as with AMD's RYZEN_CORES_THRESHOLD.
constexpr inline u32 smt_below_cores = 8;

// Returns the fastest cores on the largest level-3 caches that affinity_mask allows, none when it is 0.
// Without cores, each allowed logical processor counts as one. Without caches, all cores share one.
thread_pool_cpus pick_thread_pool_cpus(ArrayView<const cpu_core> cores, ArrayView<const l3_cache> l3_caches,
                                       u64 affinity_mask) noexcept;
// Returns pick_thread_pool_cpus() of this machine's cores and caches and process_affinity_mask(). On Windows,
// only the calling thread's processor group counts.
thread_pool_cpus query_thread_pool_cpus() noexcept;

// Returns the logical processors below 64 the process may run on. Where the OS doesn't say, returns the
// first std::thread::hardware_concurrency() ones.
u64 process_affinity_mask() noexcept;
// Returns false when the OS rejects logical_cpu_mask, and on every OS but Windows and Linux.
bool set_current_thread_affinity(u64 logical_cpu_mask) noexcept;

// Returns the mask of the logical processors below 64 that a Linux cpulist such as "0-3,8-11" names, or 0
// when it doesn't parse.
u64 parse_cpu_list(StringView list) noexcept;

} // namespace floormat
