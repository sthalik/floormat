#include "slab-arena.hpp"
#include "assert.hpp"
#include "atomic.hpp"
#include "start-lifetime-as.hpp"
#include "superpage.hpp"
#include <bit>
#include <cstring>
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "strerror.hpp"
#include <cr/StringView.h>
#include <windows.h>
#ifndef MEM_REPLACE_PLACEHOLDER
#define MEM_REPLACE_PLACEHOLDER 0x00004000
#endif
#ifndef MEM_RESERVE_PLACEHOLDER
#define MEM_RESERVE_PLACEHOLDER 0x00040000
#endif
#ifndef MEM_PRESERVE_PLACEHOLDER
#define MEM_PRESERVE_PLACEHOLDER 0x00000002
#endif
#elif defined __linux__
#include <sys/mman.h>
#endif

namespace floormat::slab::detail {

namespace {

constexpr u32 superpage_bytes = 2 << 20;
constexpr u32 no_superpages = tunables::no_superpages;

usize round_up(usize x, usize multiple) { return (x + multiple - 1) / multiple * multiple; }

#ifdef _WIN32
// MinGW-w64 declares VirtualAlloc2 only for NTDDI_VERSION >= NTDDI_WIN10_RS4.
using fn_VirtualAlloc2 = PVOID (WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, void*, ULONG);

fn_VirtualAlloc2 find_VirtualAlloc2() noexcept
{
    static const fn_VirtualAlloc2 fn = [] {
        auto* const dll = GetModuleHandleA("kernelbase.dll");
        return dll ? std::bit_cast<fn_VirtualAlloc2>(GetProcAddress(dll, "VirtualAlloc2")) : nullptr;
    }();
    return fn;
}

[[noreturn]] void abort_with_win32_error(const char* what, usize bytes)
{
    char buf[128];
    const auto err = (u32)GetLastError();
    fm_abort("slab::memory_resource: %s of %zu bytes failed: %s (GetLastError 0x%08x)",
             what, bytes, get_win32_error_string(buf, err).data(), (unsigned)err);
}

// Microsoft advises against repeated large-page allocations, which can slow the system once memory is fragmented.
volatile u8 large_pages_enabled = 1;
#endif

bool superpages_enabled()
{
#ifdef _WIN32
    return sizeof(void*) == 8 && windows_enable_large_pages().page == superpage_bytes && find_VirtualAlloc2();
#elif defined __linux__
    const superpage_mode mode = linux_superpage_mode();
    return sizeof(void*) == 8 && (mode == superpage_mode::always || mode == superpage_mode::madvise);
#else
    return false;
#endif
}

void commit_superpage(arena& a, u32 i)
{
    char* const p = a.base + a.bytes_until_superpages + (usize)i * superpage_bytes;
#ifdef _WIN32
    constexpr ULONG flags = MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER;
    if (!atomic_load(&large_pages_enabled, memory_order::relaxed) ||
        !find_VirtualAlloc2()(nullptr, p, superpage_bytes, flags | MEM_LARGE_PAGES, PAGE_READWRITE, nullptr, 0))
    {
        atomic_store(&large_pages_enabled, 0, memory_order::relaxed);
        if (!find_VirtualAlloc2()(nullptr, p, superpage_bytes, flags, PAGE_READWRITE, nullptr, 0))
            abort_with_win32_error("commit", superpage_bytes);
    }
#else
    a.memory.commit((usize)(p - a.memory.data()), superpage_bytes);
#if defined __linux__ && defined MADV_HUGEPAGE
    if (linux_superpage_mode() == superpage_mode::madvise)
        (void)madvise(p, superpage_bytes, MADV_HUGEPAGE);
#endif
#endif
}

// Commits the bytes [begin, end) from a.base below a.bytes_until_superpages and the 2 MiB pages
// [first_superpage, num_superpages) past it. Returns once every page below num_superpages is committed.
void commit_arena_bytes(arena& a, usize begin, usize end, u32 first_superpage, u32 num_superpages)
{
    const usize base_offset = (usize)(a.base - a.memory.data());
    if (a.bytes_until_superpages == no_superpages || end <= a.bytes_until_superpages)
        a.memory.commit(base_offset + begin, end - begin);
    else
    {
        if (begin < a.bytes_until_superpages)
            a.memory.commit(base_offset + begin, a.bytes_until_superpages - begin);
        for (u32 i = first_superpage; i < num_superpages; i++)
            commit_superpage(a, i);
        for (u32 num; (num = atomic_load(&a.num_committed_superpages, memory_order::acquire)) < first_superpage;)
            atomic_wait(&a.num_committed_superpages, num, memory_order::acquire);
        if (first_superpage < num_superpages)
        {
            atomic_store(&a.num_committed_superpages, num_superpages, memory_order::release);
            if (a.thread_model == thread_model::multi_threaded)
                atomic_notify_all(&a.num_committed_superpages);
        }
    }
}

constexpr u32 bytes_per_pagemap_entry = arena::bytes_per_pagemap_entry;
constexpr u32 extent_start_mask = (1u << arena::num_extent_start_bits) - 1;
constexpr u32 no_free_extent = (u32)-1;
constexpr u32 min_free_extent_size_log2 = std::countr_zero(bytes_per_pagemap_entry / 16);
constexpr u32 num_free_lists_per_first_level_index_log2 =
    std::countr_zero(arena::num_free_lists_per_first_level_index);

// The first 16 bytes of a free extent, whose last 16 hold its start. Offsets and sizes are in 16-byte units.
struct free_extent_header
{
    u32 size, prev, next;
};

// In bytes from the arena's base.
struct extent_bounds
{
    u64 start, end;
};

struct free_list_index
{
    u32 first_level_index, second_level_index;
};

// Free extents are poisoned under ASan, except while the free extent lists access them.
template<typename T>
T load_from_free_extent(const char* p)
{
    T x;
#if fm_ASAN
    __asan_unpoison_memory_region(p, sizeof(T));
#endif
    std::memcpy(&x, p, sizeof(T));
#if fm_ASAN
    __asan_poison_memory_region(p, sizeof(T));
#endif
    return x;
}

template<typename T>
void store_into_free_extent(char* p, const T& x)
{
#if fm_ASAN
    __asan_unpoison_memory_region(p, sizeof(T));
#endif
    std::memcpy(p, &x, sizeof(T));
#if fm_ASAN
    __asan_poison_memory_region(p, sizeof(T));
#endif
}

u32 load_pagemap_entry(const arena& a, u64 unit)
{
    return atomic_load(&a.pagemap[unit], memory_order::relaxed);
}

void store_pagemap_entry(arena& a, u64 unit, u32 entry)
{
    atomic_store(&a.pagemap[unit], entry, memory_order::relaxed);
}

u32 slab_index(u32 entry)
{
    return entry >> arena::num_extent_start_bits;
}

// Returns where an extent begins inside the unit, in bytes from the arena's base, or 0 when none does.
u64 extent_start_in_unit(u32 entry, u64 unit)
{
    const u32 start = entry & extent_start_mask;
    return start ? unit * bytes_per_pagemap_entry + start * 16 : 0;
}

// Writes slab into the entries of units whose first byte lies in [start, end), and marks where the extent begins
// and ends. A lock-free deallocate() may read an entry at any time, so each store writes its final value.
void write_extent_pagemap_entries(arena& a, u64 start, u64 end, u32 slab)
{
    const u32 entry = slab << arena::num_extent_start_bits;
    for (u64 u = (start + bytes_per_pagemap_entry - 1) / bytes_per_pagemap_entry;
         u < end / bytes_per_pagemap_entry; u++)
        store_pagemap_entry(a, u, entry);
    if (end % bytes_per_pagemap_entry)
        store_pagemap_entry(a, end / bytes_per_pagemap_entry, entry | (u32)(end % bytes_per_pagemap_entry / 16));
    if (start % bytes_per_pagemap_entry)
    {
        const u64 u = start / bytes_per_pagemap_entry;
        store_pagemap_entry(a, u, (load_pagemap_entry(a, u) & ~extent_start_mask)
                                  | (u32)(start % bytes_per_pagemap_entry / 16));
    }
}

// Returns the extent holding [first, first + bytes), whose unused bytes on either side number below
// bytes_per_pagemap_entry.
extent_bounds find_extent(const arena& a, u64 first, u64 bytes)
{
    const u64 bump = (u64)a.bump_offset * 16;
    const u64 u = first / bytes_per_pagemap_entry;
    u64 start = extent_start_in_unit(load_pagemap_entry(a, u), u);
    if (!start && u > 0)
        start = extent_start_in_unit(load_pagemap_entry(a, u - 1), u - 1);
    if (!start)
        start = u * bytes_per_pagemap_entry;
    const u64 last = first + bytes, v = last / bytes_per_pagemap_entry;
    u64 end;
    if (last >= bump)
        end = last;
    else if (const u64 x = extent_start_in_unit(load_pagemap_entry(a, v), v))
        end = x;
    else if (last % bytes_per_pagemap_entry == 0)
        end = last;
    else if ((v + 1) * bytes_per_pagemap_entry >= bump)
        end = (v + 1) * bytes_per_pagemap_entry;
    else if (const u64 x = extent_start_in_unit(load_pagemap_entry(a, v + 1), v + 1))
        end = x;
    else
        end = (v + 1) * bytes_per_pagemap_entry;
    fm_assert(start <= first && first - start < bytes_per_pagemap_entry);
    fm_assert(end >= last && end - last < bytes_per_pagemap_entry && end <= bump);
    return {start, end};
}

free_list_index free_list_of_size(u64 size)
{
    constexpr u32 num_first_level_indexes = arena::num_first_level_indexes;
    constexpr u32 second_level_mask = arena::num_free_lists_per_first_level_index - 1;
    const u32 log2 = (u32)std::bit_width(size) - 1;
    fm_debug_assert(log2 >= min_free_extent_size_log2);
    if (log2 - min_free_extent_size_log2 >= num_first_level_indexes)
        return {num_first_level_indexes - 1, second_level_mask};
    else
        return {log2 - min_free_extent_size_log2,
                (u32)(size >> (log2 - num_free_lists_per_first_level_index_log2)) & second_level_mask};
}

free_extent_header load_free_extent_header(const arena& a, u32 start)
{
    return load_from_free_extent<free_extent_header>(a.base + (usize)start * 16);
}

void store_free_extent_header(arena& a, u32 start, const free_extent_header& h)
{
    store_into_free_extent(a.base + (usize)start * 16, h);
}

// The bounds are in 16-byte units.
void insert_into_free_list(arena& a, u32 start, u32 end)
{
    const auto [fl, sl] = free_list_of_size(end - start);
    const bool is_list_empty = !(a.free_extent_lists.second_level_bitmaps[fl] & 1u << sl);
    const u32 next = is_list_empty ? no_free_extent : a.free_extent_lists.first_extents[fl][sl];
    store_free_extent_header(a, start, {end - start, no_free_extent, next});
    store_into_free_extent(a.base + (usize)end * 16 - 16, start);
    if (next != no_free_extent)
    {
        free_extent_header h = load_free_extent_header(a, next);
        h.prev = start;
        store_free_extent_header(a, next, h);
    }
    a.free_extent_lists.first_extents[fl][sl] = start;
    a.free_extent_lists.second_level_bitmaps[fl] |= 1u << sl;
    a.free_extent_lists.first_level_bitmap |= 1u << fl;
}

void remove_from_free_list(arena& a, u32 start)
{
    const free_extent_header h = load_free_extent_header(a, start);
    const auto [fl, sl] = free_list_of_size(h.size);
    if (h.prev != no_free_extent)
    {
        free_extent_header prev = load_free_extent_header(a, h.prev);
        prev.next = h.next;
        store_free_extent_header(a, h.prev, prev);
    }
    else
        a.free_extent_lists.first_extents[fl][sl] = h.next;
    if (h.next != no_free_extent)
    {
        free_extent_header next = load_free_extent_header(a, h.next);
        next.prev = h.prev;
        store_free_extent_header(a, h.next, next);
    }
    else if (h.prev == no_free_extent)
    {
        a.free_extent_lists.second_level_bitmaps[fl] &= ~(1u << sl);
        if (!a.free_extent_lists.second_level_bitmaps[fl])
            a.free_extent_lists.first_level_bitmap &= ~(1u << fl);
    }
}

// Returns a free extent of at least size 16-byte units, taken off its list, or {0, 0} when none is that large.
// As with TLSF's mapping_search(), an extent on the size's own list may be missed.
extent_bounds take_free_extent(arena& a, u64 size)
{
    const u32 log2 = (u32)std::bit_width(size) - 1;
    const u64 rounded = size + (u64{1} << (log2 - num_free_lists_per_first_level_index_log2)) - 1;
    fm_assert((u32)std::bit_width(rounded) - 1 - min_free_extent_size_log2
              < arena::num_first_level_indexes);
    auto [fl, sl] = free_list_of_size(rounded);
    u32 sl_bitmap = a.free_extent_lists.second_level_bitmaps[fl] & ~0u << sl;
    if (!sl_bitmap)
    {
        const u32 fl_bitmap = fl + 1 < arena::num_first_level_indexes
                              ? a.free_extent_lists.first_level_bitmap & ~0u << (fl + 1) : 0;
        if (!fl_bitmap)
            return {};
        fl = (u32)std::countr_zero(fl_bitmap);
        sl_bitmap = a.free_extent_lists.second_level_bitmaps[fl];
    }
    sl = (u32)std::countr_zero(sl_bitmap);
    const u32 start = a.free_extent_lists.first_extents[fl][sl];
    const u32 extent_size = load_free_extent_header(a, start).size;
    remove_from_free_list(a, start);
    return {(u64)start * 16, ((u64)start + extent_size) * 16};
}

// Puts the extent [start, end) on a free list, merged with the free extents on either side.
// TODO: decommit free extents on Linux.
void put_extent_on_free_list(arena& a, u64 start, u64 end)
{
#if fm_ASAN
    __asan_poison_memory_region(a.base + start, end - start);
#endif
    constexpr u32 free_entry = arena::free_extent_marker << arena::num_extent_start_bits;
    u64 merged_start = start, merged_end = end;
    if (start > 0 && slab_index(load_pagemap_entry(a, (start - 1) / bytes_per_pagemap_entry))
                     == arena::free_extent_marker)
    {
        merged_start = (u64)load_from_free_extent<u32>(a.base + start - 16) * 16;
        remove_from_free_list(a, (u32)(merged_start / 16));
    }
    if (end < (u64)a.bump_offset * 16 &&
        slab_index(load_pagemap_entry(a, (end + bytes_per_pagemap_entry - 1) / bytes_per_pagemap_entry))
        == arena::free_extent_marker)
    {
        merged_end = end + (u64)load_free_extent_header(a, (u32)(end / 16)).size * 16;
        remove_from_free_list(a, (u32)(end / 16));
    }
    for (u64 u = (start + bytes_per_pagemap_entry - 1) / bytes_per_pagemap_entry;
         u < end / bytes_per_pagemap_entry; u++)
        store_pagemap_entry(a, u, free_entry);
    if (end % bytes_per_pagemap_entry)
        store_pagemap_entry(a, end / bytes_per_pagemap_entry,
                            free_entry | (merged_end == end ? (u32)(end % bytes_per_pagemap_entry / 16) : 0));
    if (start % bytes_per_pagemap_entry && merged_start < start)
        store_pagemap_entry(a, start / bytes_per_pagemap_entry, free_entry);
    insert_into_free_list(a, (u32)(merged_start / 16), (u32)(merged_end / 16));
}

// Returns a free extent holding bytes at a multiple of alignment, taken off its list, or {0, 0} when none does.
extent_bounds take_fitting_free_extent(arena& a, u32 bytes, u32 alignment)
{
    const u32 padding = alignment > 16 ? alignment - 16 : 0;
    // take_free_extent() may skip the list holding extents of bytes' own size, such as a pool's given-back slab.
    const auto [fl, sl] = free_list_of_size(bytes / 16);
    if (!(a.free_extent_lists.second_level_bitmaps[fl] & 1u << sl))
        return take_free_extent(a, ((u64)bytes + padding) / 16);
    else
    {
        const u32 start = a.free_extent_lists.first_extents[fl][sl];
        const u64 end = ((u64)start + load_free_extent_header(a, start).size) * 16;
        if (((u64)start * 16 + alignment - 1) / alignment * alignment + bytes <= end)
        {
            remove_from_free_list(a, start);
            return {(u64)start * 16, end};
        }
        else
            return take_free_extent(a, ((u64)bytes + padding) / 16);
    }
}

// The first bytes of an uncoalesced array extent. Offsets are in 16-byte units from the arena's base.
struct uncoalesced_array_extent_header
{
    u32 next, start, end;
};

// The bytes are those allocate_extent() got for the array extent at first.
void push_uncoalesced_array_extent(arena& a, u32 first, u32 bytes)
{
    const u64 first_byte = (u64)first * 16;
    const extent_bounds e = find_extent(a, first_byte, bytes);
#if fm_ASAN
    __asan_poison_memory_region(a.base + e.start, e.end - e.start);
#endif
    const auto [fl, sl] = free_list_of_size(bytes / 16);
    u32& head = a.first_uncoalesced_array_extents[fl][sl];
    store_into_free_extent(a.base + first_byte,
                           uncoalesced_array_extent_header{head, (u32)(e.start / 16), (u32)(e.end / 16)});
    head = first;
    a.num_uncoalesced_array_extents++;
}

// Returns the offset in 16-byte units of bytes at a multiple of alignment, in the first uncoalesced array extent
// on the list for bytes, taken off it, or (u32)-1 when that extent can't hold them.
u32 take_uncoalesced_array_extent(arena& a, u32 bytes, u32 alignment)
{
    const auto [fl, sl] = free_list_of_size(bytes / 16);
    u32& head = a.first_uncoalesced_array_extents[fl][sl];
    if (head == (u32)-1)
        return (u32)-1;
    else
    {
        const auto h = load_from_free_extent<uncoalesced_array_extent_header>(a.base + (usize)head * 16);
        const u64 start = (u64)h.start * 16, end = (u64)h.end * 16;
        const u64 first = (start + alignment - 1) / alignment * alignment, last = first + bytes;
        // find_extent() must find the same bounds from the new first and bytes.
        if (first - start >= bytes_per_pagemap_entry || last > end || end - last >= bytes_per_pagemap_entry)
            return (u32)-1;
        else
        {
            head = h.next;
            a.num_uncoalesced_array_extents--;
            return (u32)(first / 16);
        }
    }
}

void coalesce_array_extents(arena& a)
{
    for (u32 fl = 0; fl < arena::num_first_level_indexes; fl++)
        for (u32 sl = 0; sl < arena::num_free_lists_per_first_level_index; sl++)
        {
            u32 first = a.first_uncoalesced_array_extents[fl][sl];
            a.first_uncoalesced_array_extents[fl][sl] = (u32)-1;
            while (first != (u32)-1)
            {
                const auto h = load_from_free_extent<uncoalesced_array_extent_header>(a.base + (usize)first * 16);
                put_extent_on_free_list(a, (u64)h.start * 16, (u64)h.end * 16);
                first = h.next;
            }
        }
    a.num_uncoalesced_array_extents = 0;
}

// The first bytes of an array extent on the list that arena::first_pending_array_extent starts.
struct pending_array_extent_header
{
    u32 next, bytes;
};

void free_pending_array_extents(arena& a)
{
    if (atomic_load(&a.first_pending_array_extent, memory_order::relaxed) != (u32)-1)
    {
        u32 first = atomic_exchange(&a.first_pending_array_extent, (u32)-1, memory_order::acquire);
        while (first != (u32)-1)
        {
            const u64 first_byte = (u64)first * 16;
            // Read before push_uncoalesced_array_extent() overwrites it.
            const auto h = load_from_free_extent<pending_array_extent_header>(a.base + first_byte);
            fm_assert(slab_index(load_pagemap_entry(a, (first_byte + bytes_per_pagemap_entry - 1)
                                                       / bytes_per_pagemap_entry))
                      == arena::array_extent_marker);
            push_uncoalesced_array_extent(a, first, h.bytes);
            first = h.next;
        }
    }
}

} // namespace

arena::~arena() noexcept
{
#if fm_ASAN
    // ASan on Windows doesn't intercept VirtualFree, so the slabs would stay poisoned after the release.
    if (base)
        __asan_unpoison_memory_region(base, (usize)bump_offset * 16);
#endif
#ifdef _WIN32
    // memory releases only the allocation at its start.
    if (base && bytes_until_superpages != no_superpages)
        for (u32 i = 0; i <= num_committed_superpages; i++)
        {
            char* const p = base + bytes_until_superpages + (usize)i * superpage_bytes;
            const usize bytes = i < num_committed_superpages ? superpage_bytes
                                                             : (usize)(memory.data() + memory.size() - p);
            if (p != memory.data() && !VirtualFree(p, 0, MEM_RELEASE))
                abort_with_win32_error("release", bytes);
        }
#endif
}

void reserve_arena_addr_range(arena& a)
{
    fm_assert(!a.base);
    const u64 max_offset = a.arena_reservation_bytes / 16;
    a.max_bump_offset = max_offset < (u32)-1 ? (u32)max_offset : (u32)-1;
    const usize usable_bytes = (usize)a.max_bump_offset * 16;
    const u64 boundary_offset = a.bytes_until_superpages == no_superpages ? 0 : a.bytes_until_superpages;
    usize bytes_from_base = a.arena_reservation_bytes;
    if (a.bytes_until_superpages != no_superpages && a.bytes_until_superpages < usable_bytes)
        bytes_from_base = a.bytes_until_superpages
                          + round_up(usable_bytes - a.bytes_until_superpages, superpage_bytes);
    else
        a.bytes_until_superpages = no_superpages;
    if (a.bytes_until_superpages != no_superpages && !superpages_enabled())
        a.bytes_until_superpages = no_superpages;
    const usize reservation_bytes = bytes_from_base + superpage_bytes;

#ifdef _WIN32
    if (a.bytes_until_superpages != no_superpages)
    {
        auto* const data = (char*)find_VirtualAlloc2()(nullptr, nullptr, reservation_bytes,
                                                         MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
                                                         nullptr, 0);
        if (!data)
            abort_with_win32_error("placeholder reservation", reservation_bytes);
        a.memory = virtual_memory{data, reservation_bytes};
    }
    else
        a.memory.reserve(reservation_bytes);
#else
    a.memory.reserve(reservation_bytes);
#endif
    const u64 data = (uptr)a.memory.data();
    a.base = a.memory.data() + (superpage_bytes - (data + boundary_offset) % superpage_bytes) % superpage_bytes;
#if defined __linux__ && defined MADV_NOHUGEPAGE
    const usize bytes_below_superpages = a.bytes_until_superpages == no_superpages
                                         ? reservation_bytes
                                         : (usize)(a.base + a.bytes_until_superpages - a.memory.data());
    (void)madvise(a.memory.data(), bytes_below_superpages, MADV_NOHUGEPAGE);
#endif
#ifdef _WIN32
    if (a.bytes_until_superpages != no_superpages)
    {
        const usize bytes_below_superpages = (usize)(a.base + a.bytes_until_superpages - a.memory.data());
        if (bytes_below_superpages > 0)
        {
            if (!VirtualFree(a.memory.data(), bytes_below_superpages, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
                abort_with_win32_error("placeholder split", bytes_below_superpages);
            if (!find_VirtualAlloc2()(nullptr, a.memory.data(), bytes_below_superpages,
                                       MEM_RESERVE | MEM_REPLACE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0))
                abort_with_win32_error("reservation", bytes_below_superpages);
        }
    }
#endif
    a.pagemap_memory.reserve(a.arena_reservation_bytes / arena::bytes_per_pagemap_entry * sizeof(u32));
    a.pagemap = reinterpret_cast<u32*>(a.pagemap_memory.data());
    for (auto& lists : a.first_uncoalesced_array_extents)
        for (u32& first : lists)
            first = (u32)-1;
}

u32 allocate_extent(arena& a, u32 bytes, u32 alignment, u32 slab)
{
    const bool is_single_threaded = a.thread_model == thread_model::single_threaded;
    const bool is_multi_threaded = a.thread_model == thread_model::multi_threaded;
    if (is_multi_threaded)
        a.lock.lock();
    if (!a.base) [[unlikely]]
        reserve_arena_addr_range(a);
    if (!is_single_threaded)
        free_pending_array_extents(a);
    if (slab == arena::array_extent_marker)
    {
        if (is_single_threaded)
            a.num_allocated_array_extents++;
        else
            atomic_fetch_add(&a.num_allocated_array_extents, 1u, memory_order::relaxed);
        const u32 first = take_uncoalesced_array_extent(a, bytes, alignment);
        if (first != (u32)-1)
        {
            if (is_multi_threaded)
                a.lock.unlock();
            return first;
        }
    }
    extent_bounds f = take_fitting_free_extent(a, bytes, alignment);
    if (!f.end && a.num_uncoalesced_array_extents)
    {
        coalesce_array_extents(a);
        f = take_fitting_free_extent(a, bytes, alignment);
    }
    if (!f.end && atomic_load(&a.needs_empty_slab_check, memory_order::relaxed)
        && atomic_exchange(&a.needs_empty_slab_check, u8{0}, memory_order::acquire))
    {
        const auto give_back_empty_slabs = a.give_back_empty_slabs;
        void* const resource = a.resource;
        // Pools give slabs back through deallocate_extent(), which takes a.lock.
        if (is_multi_threaded)
            a.lock.unlock();
        give_back_empty_slabs(resource);
        if (is_multi_threaded)
            a.lock.lock();
        f = take_fitting_free_extent(a, bytes, alignment);
    }
    if (f.end)
    {
        const u64 first = (f.start + alignment - 1) / alignment * alignment, last = first + bytes;
        const u64 start = first - f.start >= bytes_per_pagemap_entry ? first : f.start;
        const u64 end = f.end - last >= bytes_per_pagemap_entry ? last : f.end;
        write_extent_pagemap_entries(a, start, end, slab);
        if (start > f.start)
            insert_into_free_list(a, (u32)(f.start / 16), (u32)(start / 16));
        if (end < f.end)
            insert_into_free_list(a, (u32)(end / 16), (u32)(f.end / 16));
        if (is_multi_threaded)
            a.lock.unlock();
        return (u32)(first / 16);
    }

    const u64 start = (u64)a.bump_offset * 16;
    const u64 first = (start + alignment - 1) / alignment * alignment;
    const u64 end = first + bytes;
    if (end > (u64)a.max_bump_offset * 16) [[unlikely]]
        fm_abort("slab::memory_resource: %zu-byte reservation used up", a.arena_reservation_bytes);
    const u32 first_superpage = a.num_superpage_commits_started;
    const u32 num_superpages = a.bytes_until_superpages == no_superpages || end <= a.bytes_until_superpages
                               ? 0 : (u32)(round_up((usize)end - a.bytes_until_superpages, superpage_bytes)
                                           / superpage_bytes);
    if (first_superpage < num_superpages)
    {
#ifdef _WIN32
        // Splitting a range that is already a whole placeholder fails, so splits run in address order.
        for (u32 i = first_superpage; i < num_superpages; i++)
        {
            char* const p = a.base + a.bytes_until_superpages + (usize)i * superpage_bytes;
            if (!VirtualFree(p, superpage_bytes, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
                abort_with_win32_error("placeholder split", superpage_bytes);
        }
#endif
        a.num_superpage_commits_started = num_superpages;
    }

    const u32 first_unit = (u32)((start + bytes_per_pagemap_entry - 1) / bytes_per_pagemap_entry);
    const u32 end_unit = (u32)((end + bytes_per_pagemap_entry - 1) / bytes_per_pagemap_entry);
    const usize page = virtual_memory::page_size();
    const usize commit_begin = round_up((usize)first_unit * sizeof(u32), page);
    const usize commit_end = round_up((usize)end_unit * sizeof(u32), page);
    if (commit_begin < commit_end)
    {
        a.pagemap_memory.commit(commit_begin, commit_end - commit_begin);
        (void)start_lifetime_as_array<u32>(a.pagemap_memory.data() + commit_begin,
                                           (commit_end - commit_begin) / sizeof(u32));
    }
    // A free extent's header can't be written before its commit, so the bytes before first stay in use until then.
    const bool has_free_front = first - start >= bytes_per_pagemap_entry;
    if (has_free_front)
        write_extent_pagemap_entries(a, start, first, arena::extent_commit_marker);
    write_extent_pagemap_entries(a, has_free_front ? first : start, end, slab);
#if fm_ASAN
    if (!has_free_front)
        __asan_poison_memory_region(a.base + start, first - start);
#endif
    atomic_store(&a.bump_offset, (u32)(end / 16), memory_order::relaxed);
    if (is_multi_threaded)
        a.lock.unlock();
    commit_arena_bytes(a, (usize)start, (usize)end, first_superpage, num_superpages);
    if (has_free_front)
    {
        if (is_multi_threaded)
            a.lock.lock();
        put_extent_on_free_list(a, start, first);
        if (is_multi_threaded)
            a.lock.unlock();
    }
    return (u32)(first / 16);
}

void deallocate_extent(arena& a, u32 first, u32 bytes, u32 slab)
{
    const bool is_multi_threaded = a.thread_model == thread_model::multi_threaded;
    if (is_multi_threaded)
        a.lock.lock();
    const u64 first_byte = (u64)first * 16;
    fm_assert(first_byte + bytes <= (u64)a.bump_offset * 16);
    fm_assert(slab_index(load_pagemap_entry(a, (first_byte + bytes_per_pagemap_entry - 1)
                                               / bytes_per_pagemap_entry)) == slab);
    if (slab == arena::array_extent_marker)
    {
        fm_assert(a.thread_model == thread_model::single_threaded);
        a.num_allocated_array_extents--;
        push_uncoalesced_array_extent(a, first, bytes);
    }
    else
    {
        const extent_bounds e = find_extent(a, first_byte, bytes);
        put_extent_on_free_list(a, e.start, e.end);
    }
    if (is_multi_threaded)
        a.lock.unlock();
}

void push_pending_array_extent(arena& a, u32 first, u32 bytes)
{
    fm_assert(a.thread_model != thread_model::single_threaded);
    const u64 first_byte = (u64)first * 16;
    fm_assert(first_byte + bytes <= (u64)atomic_load(&a.bump_offset, memory_order::relaxed) * 16);
    fm_assert(slab_index(load_pagemap_entry(a, (first_byte + bytes_per_pagemap_entry - 1)
                                               / bytes_per_pagemap_entry))
              == arena::array_extent_marker);
    atomic_fetch_sub(&a.num_allocated_array_extents, 1u, memory_order::relaxed);
    char* const p = a.base + first_byte;
#if fm_ASAN
    __asan_poison_memory_region(p, bytes);
#endif
    pending_array_extent_header h{atomic_load(&a.first_pending_array_extent, memory_order::relaxed), bytes};
    do
        store_into_free_extent(p, h);
    while (!atomic_compare_exchange_weak(&a.first_pending_array_extent, h.next, first, memory_order::release));
}

} // namespace floormat::slab::detail
