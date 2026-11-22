#pragma once
#include "defs.hpp"
#include "integer-types.hpp"
#include "spinlock.hpp"
#include "virtual-memory.hpp"
#include <new>
#include <version>

namespace floormat::slab {

enum class thread_model : u8
{
    single_threaded,
    // One thread allocates from all of the resource's pools, and any thread deallocates.
    single_allocating_thread,
    multi_threaded,
};

enum class buffer_source : u8
{
    slabs,
    guard_pages,
    operator_new,
};

struct tunables
{
    static constexpr u32 no_superpages = (u32)-1;

    // A power of two, at most 64 GiB.
    usize arena_reservation_bytes = sizeof(void*) == 8 ? usize{64} << 30 : usize{64} << 20;
    // From 8 KiB to 2 MiB. An object whose rounded size is larger is allocated as an array.
    u32 max_slab_bytes = 64 << 10;
    // From max_slab_bytes to 32 MiB. An array whose rounded size is larger gets a mapping of its own.
    u32 max_array_extent_bytes = sizeof(void*) == 8 ? 32 << 20 : 2 << 20;
    // A multiple of 64 KiB, or no_superpages. The arena's base lies this many bytes below a 2 MiB boundary.
    u32 bytes_until_superpages = 1 << 20;
    slab::thread_model thread_model = slab::thread_model::single_threaded;
    bool is_slab_of_last_deallocation_cached = true;
    slab::buffer_source buffer_source = fm_ASAN ? slab::buffer_source::operator_new : slab::buffer_source::slabs;
    bool has_pool_per_type = true;
    // When true, pools give back every empty slab but one. Otherwise the arena takes them back before it grows.
    bool is_empty_slab_given_back = false;
};

} // namespace floormat::slab

namespace floormat::slab::detail {

#ifdef __cpp_lib_hardware_interference_size
#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winterference-size"
#endif
constexpr inline usize hardware_destructive_interference_size = std::hardware_destructive_interference_size;
constexpr inline usize hardware_constructive_interference_size = std::hardware_constructive_interference_size;
#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic pop
#endif
#else
constexpr inline usize hardware_destructive_interference_size = 64;
constexpr inline usize hardware_constructive_interference_size = 64;
#endif

// The address range a memory_resource takes the slabs of all its pools from. An extent is a contiguous run
// of its bytes, free or holding one array or one slab, with fewer than 4096 unused bytes before and after it.
struct arena
{
    static constexpr u32 bytes_per_pagemap_entry = 4096;
    // A pagemap entry holds a slab index above its low bits, and in them where in its unit the next extent
    // begins, in 16-byte units, or 0 when no extent begins inside the unit.
    static constexpr u32 num_extent_start_bits = 8;
    // A marker is a value pagemap entries hold in place of a slab index. These mark units whose first byte lies
    // in a free extent, in an array, or in the free bytes before an extent that allocate_extent() is committing.
    static constexpr u32 free_extent_marker = (1u << (32 - num_extent_start_bits)) - 1;
    static constexpr u32 array_extent_marker = free_extent_marker - 1;
    static constexpr u32 extent_commit_marker = free_extent_marker - 2;
    // As in TLSF, first-level index i covers free extents of [4 << i, 8 << i) KiB, a range its lists split evenly.
    // Extents of 64 MiB or more go on the last list, since no request exceeds 32 MiB plus alignment padding.
    static constexpr u32 num_first_level_indexes = 14, num_free_lists_per_first_level_index = 32;

    virtual_memory memory, pagemap_memory;
    char* base = nullptr;
    // Its entry u describes the bytes from base + u * bytes_per_pagemap_entry.
    u32* pagemap = nullptr;
    // In 16-byte units from base. Lock-free deallocate() reads it.
    u32 bump_offset = 0, max_bump_offset = 0;
    // reserve_arena_addr_range() sets it to no_superpages when the system isn't expected to back 2 MiB pages.
    u32 bytes_until_superpages;
    // 2 MiB pages past bytes_until_superpages whose commit began. The first num_committed_superpages are done.
    u32 num_superpage_commits_started = 0, num_committed_superpages = 0;
    usize arena_reservation_bytes;
    // Only multi_threaded takes it, in pool creation, allocate_extent() and deallocate_extent().
    Spinlock lock;
    slab::thread_model thread_model;
    u32 num_allocated_array_extents = 0;
    struct {
        // Bit i is set when a list of first-level index i holds an extent.
        u32 first_level_bitmap = 0;
        // Bit j of element i is set when list j of first-level index i holds an extent.
        u32 second_level_bitmaps[num_first_level_indexes] = {};
        // In 16-byte units from base. Valid while the list's bit is set.
        u32 first_extents[num_first_level_indexes][num_free_lists_per_first_level_index] = {};
    } free_extent_lists;
    // Freed array extents whose coalescing is deferred, on one list per free list of their bytes, starting here
    // in 16-byte units from base or empty at (u32)-1. allocate_extent() coalesces them before growing the arena.
    u32 first_uncoalesced_array_extents[num_first_level_indexes][num_free_lists_per_first_level_index] = {};
    u32 num_uncoalesced_array_extents = 0;
    // Gives back the empty slabs of the pools of resource, the memory_resource that owns the arena.
    void (*give_back_empty_slabs)(void* resource) noexcept = nullptr;
    void* resource = nullptr;
    // 1 when a pool may hold an empty slab. Pools set it without a.lock.
    u8 needs_empty_slab_check = 0;
    // The list of array extents waiting for the next allocate_extent() to free them starts here, in 16-byte units
    // from base, or is empty when this is (u32)-1.
    alignas(hardware_destructive_interference_size) u32 first_pending_array_extent = (u32)-1;

    explicit constexpr arena(const tunables& tunables) noexcept;
    ~arena() noexcept;
    fm_DISABLE_MOVE_COPY(arena);
};

constexpr arena::arena(const tunables& tunables) noexcept:
    bytes_until_superpages{tunables.bytes_until_superpages},
    arena_reservation_bytes{tunables.arena_reservation_bytes},
    thread_model{tunables.thread_model}
{
}

void reserve_arena_addr_range(arena& a);
// Returns the offset from a.base in 16-byte units of bytes at a multiple of alignment, in an extent whose
// pagemap entries hold slab, a slab index or array_extent_marker. Never call it under a.lock.
u32 allocate_extent(arena& a, u32 bytes, u32 alignment, u32 slab);
// Puts the extent that allocate_extent() returned first for back on the free extent lists.
// Never call it under a.lock.
void deallocate_extent(arena& a, u32 first, u32 bytes, u32 slab);
// Puts the array extent that allocate_extent() returned first for on the list that the next allocate_extent()
// frees. Any thread may call it, under the thread models with lock-free deallocation.
void push_pending_array_extent(arena& a, u32 first, u32 bytes);

} // namespace floormat::slab::detail
