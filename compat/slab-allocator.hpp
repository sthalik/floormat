#pragma once
#include "defs.hpp"
#include "integer-types.hpp"
#include "assert.hpp"
#include "spinlock.hpp"
#include "slab-arena.hpp"
#include "virtual-memory.hpp"
#include <array>
#include <new>
#include <type_traits>
#include <version>
#include "guard-page-memory-resource.hpp"

// Include slab-allocator.inl before naming a specialization, or Clang 24 and GCC 16 drop attributes it defines.
// https://github.com/llvm/llvm-project/issues/20160

namespace floormat::slab::detail {

struct null_mutex
{
    void lock() noexcept;
    [[nodiscard]] bool try_lock() noexcept;
    void unlock() noexcept;
};

struct pool_constants
{
    static constexpr u32 max_nonfull_slab_bitmap_levels = 4, max_given_back_slab_bitmap_levels = 4;
    // The pool object takes these bytes at the start of the pool's reservation.
    static constexpr u32 pool_object_bytes = hardware_destructive_interference_size <= 128 ? 1024 : 2048;
    // The most bytes the pool object and the nonfull slab bitmap take, at a 64 GiB reservation.
    static constexpr u32 max_nonfull_slab_bitmap_end = pool_object_bytes + ((1 << 18) + (1 << 12) + (1 << 6) + 1) * 8;
    // The given-back slab bitmap follows the nonfull one.
    // It is no larger, since a slab spans at least one pagemap unit.
    static constexpr u32 max_given_back_slab_bitmap_end =
        max_nonfull_slab_bitmap_end + ((1 << 18) + (1 << 12) + (1 << 6) + 1) * 8;

    u32 buffer_bytes = 0, buffer_align = 0, num_buffers_per_slab = 0, num_bitmap_words_per_slab = 0;
    u32 buffer_index_multiplier = 0;
    u32 slab_bytes = 0, max_slabs = 0, num_nonfull_slab_bitmap_levels = 0, num_given_back_slab_bitmap_levels = 0;
    // In u64 words from the start of the pool's reservation. In both bitmaps level 0 lies last.
    u32 nonfull_slab_bitmap_level_offset[max_nonfull_slab_bitmap_levels] = {};
    u32 given_back_slab_bitmap_level_offset[max_given_back_slab_bitmap_levels] = {};
    u32 free_bitmap_bytes = 0, deallocation_bitmap_bytes = 0;
    u32 free_bitmaps_offset = 0, deallocation_bitmaps_offset = 0, header_reservation_bytes = 0;
    slab::thread_model thread_model = slab::thread_model::single_threaded;
    bool is_slab_of_last_deallocation_cached = true;
    bool is_empty_slab_given_back = false;

    constexpr pool_constants(usize size, usize alignment, const tunables& tunables);
    constexpr bool is_deallocation_lock_free() const;
};

constexpr pool_constants::pool_constants(usize size, usize alignment, const tunables& tunables):
    thread_model{tunables.thread_model},
    is_slab_of_last_deallocation_cached{tunables.is_slab_of_last_deallocation_cached},
    is_empty_slab_given_back{tunables.is_empty_slab_given_back}
{
    auto to_u32 = [](u64 x) {
        fm_assert(x <= (u32)-1);
        return (u32)x;
    };
    auto round_up = [](u64 x, u64 multiple) { return (x + multiple - 1) / multiple * multiple; };
    fm_assert((alignment & (alignment - 1)) == 0 && alignment <= 8192);
    const u64 align = alignment < 16 ? 16 : alignment;
    const u64 bytes = round_up(size ? size : 1, align);
    fm_assert(bytes <= tunables.max_slab_bytes);
    buffer_bytes = to_u32(bytes);
    buffer_index_multiplier = to_u32(((u64{1} << 32) + bytes - 1) / bytes);
    buffer_align = to_u32(align);
    const u64 num_buffers = tunables.max_slab_bytes / bytes < 4096 ? tunables.max_slab_bytes / bytes : 4096;
    num_buffers_per_slab = to_u32(num_buffers);
    // A power of two, so that the slow paths need few instantiations.
    num_bitmap_words_per_slab = 1;
    while (num_bitmap_words_per_slab * u64{64} < num_buffers)
        num_bitmap_words_per_slab *= 2;
    slab_bytes = to_u32(num_buffers * bytes);
    fm_assert(slab_bytes >= arena::bytes_per_pagemap_entry);

    // Offsets in 16-byte units must fit a u32.
    constexpr u64 max_arena_bytes = (u64{1} << 36) - 16;
    const u64 arena_bytes = tunables.arena_reservation_bytes < max_arena_bytes ? tunables.arena_reservation_bytes : max_arena_bytes;
    max_slabs = to_u32(arena_bytes / slab_bytes);
    fm_assert(max_slabs > 0 && max_slabs <= arena::extent_commit_marker);

    u64 level_words[max_nonfull_slab_bitmap_levels] = {};
    u64 words = tunables.arena_reservation_bytes / arena::bytes_per_pagemap_entry;
    do
    {
        fm_assert(num_nonfull_slab_bitmap_levels < max_nonfull_slab_bitmap_levels);
        words = (words + 63) / 64;
        level_words[num_nonfull_slab_bitmap_levels++] = words;
    }
    while (words > 1);
    u64 offset = pool_object_bytes / sizeof(u64);
    for (u32 i = num_nonfull_slab_bitmap_levels; i-- > 0; )
    {
        nonfull_slab_bitmap_level_offset[i] = to_u32(offset);
        offset += level_words[i];
    }
    offset *= sizeof(u64);
    fm_assert(offset <= max_nonfull_slab_bitmap_end);

    u64 given_back_level_words[max_given_back_slab_bitmap_levels] = {};
    words = max_slabs;
    do
    {
        fm_assert(num_given_back_slab_bitmap_levels < max_given_back_slab_bitmap_levels);
        words = (words + 63) / 64;
        given_back_level_words[num_given_back_slab_bitmap_levels++] = words;
    }
    while (words > 1);
    for (u32 i = num_given_back_slab_bitmap_levels; i-- > 0; )
    {
        given_back_slab_bitmap_level_offset[i] = to_u32(offset / sizeof(u64));
        offset += given_back_level_words[i] * sizeof(u64);
    }
    fm_assert(offset <= max_given_back_slab_bitmap_end);

    constexpr u64 page_bytes = 4096;
    free_bitmap_bytes = num_bitmap_words_per_slab == 1 ? 16 : 16 + num_bitmap_words_per_slab * 8;
    free_bitmaps_offset = to_u32(round_up(offset, page_bytes));
    offset = free_bitmaps_offset + round_up(u64{max_slabs} * free_bitmap_bytes, page_bytes);
    if (is_deallocation_lock_free())
    {
        deallocation_bitmap_bytes = to_u32(round_up(free_bitmap_bytes, hardware_destructive_interference_size));
        deallocation_bitmaps_offset = to_u32(offset);
        offset += round_up(u64{max_slabs} * deallocation_bitmap_bytes, page_bytes);
    }
    header_reservation_bytes = to_u32(offset);
}

constexpr bool pool_constants::is_deallocation_lock_free() const
{
    return thread_model != slab::thread_model::single_threaded;
}

constexpr inline u32 max_array_size_class_bytes = 3584;

// Returns the smallest of jemalloc's size classes holding bytes, rounded up to a multiple of alignment and of 16.
constexpr usize array_size_class_bytes(usize bytes, usize alignment)
{
    fm_assert(bytes <= max_array_size_class_bytes);
    usize spacing = 16;
    while (spacing * 8 < bytes)
        spacing *= 2;
    const usize class_bytes = ((bytes ? bytes : 1) + spacing - 1) / spacing * spacing;
    const usize align = alignment < 16 ? 16 : alignment;
    return (class_bytes + align - 1) / align * align;
}

// Entry (bytes + 15) / 16 is size_class_index(array_size_class_bytes(bytes, 16), 16), under any tunables.
constexpr inline std::array<u8, max_array_size_class_bytes / 16 + 1> array_size_class_indexes = [] {
    std::array<u8, max_array_size_class_bytes / 16 + 1> ret = {};
    for (u32 i = 0; i < ret.size(); i++)
    {
        const usize index = array_size_class_bytes(i * 16, 16) / 16 - 1;
        fm_assert(index <= (u8)-1);
        ret[i] = (u8)index;
    }
    return ret;
}();

// Bit i of words is set when the slab's buffer i is free, and bit w of summary when words[w] is nonzero.
template<u32 NumWords>
struct slab_free_bitmap
{
    // In 16-byte units from the arena's base, or (u32)-1 after the slab went back to the arena.
    u32 first_buffer_offset;
    u32 num_free_buffers;
    u64 summary;
    u64 words[NumWords];
};

template<>
struct slab_free_bitmap<1>
{
    u32 first_buffer_offset;
    u64 words[1];
};

// Bits of the buffers that lock-free deallocate() freed since their slab's last merge.
template<u32 NumWords>
struct alignas(hardware_destructive_interference_size) slab_deallocation_bitmap
{
    u32 first_buffer_offset;
    // Valid while the slab is on the list that starts at pool_state::first_slab_to_merge.
    u32 next_slab_to_merge;
    u64 summary;
    u64 words[NumWords];
};

template<>
struct alignas(hardware_destructive_interference_size) slab_deallocation_bitmap<1>
{
    u32 first_buffer_offset;
    u32 next_slab_to_merge;
    u64 words[1];
};

struct alignas(hardware_destructive_interference_size) pool_state
{
    // Read by lock-free deallocate(). After pool creation only num_slabs_created changes, atomically.
    char* arena_base = nullptr;
    const u32* pagemap = nullptr;
    detail::arena* arena = nullptr;
    void* deallocation_bitmaps = nullptr;
    u32 num_slabs_created = 0;

    // The list of slabs with deallocated bits starts here, or is empty when this is (u32)-1.
    alignas(hardware_destructive_interference_size) u32 first_slab_to_merge = (u32)-1;

    // Written only by the allocating side.
    alignas(hardware_destructive_interference_size) void* free_bitmaps = nullptr;
    // A slab's bit in level 0 is the first pagemap unit starting inside it, set when the slab has a free buffer.
    // Bit w of level i + 1 is set when word w of level i is nonzero.
    u64* nonfull_slab_bitmap[pool_constants::max_nonfull_slab_bitmap_levels] = {};
    // Bit k of level 0 is set when slab k went back to the arena and index k names no slab.
    u64* given_back_slab_bitmap[pool_constants::max_given_back_slab_bitmap_levels] = {};
    // (u32)-1 when no slab has a free buffer.
    u32 lowest_nonfull_slab = (u32)-1;
    // lowest_nonfull_slab's bit in nonfull_slab_bitmap.
    u32 lowest_nonfull_slab_unit = 0;
    u32 num_allocated_buffers = 0;
    u32 num_given_back_slabs = 0;
    // The slab the pool kept when all its buffers were last free, or (u32)-1. It may have lost a buffer since.
    u32 kept_empty_slab = (u32)-1;
    // 1 when the pool may hold an empty slab. The arena reads it without the pool's mutex.
    u8 needs_empty_slab_check = 0;
    // The slab single_threaded deallocate() last freed into, and its first_buffer_offset, which is (u32)-1
    // before the first deallocate() and after that slab went back to the arena.
    u32 slab_of_last_deallocation = (u32)-1;
    u32 first_buffer_offset_of_last_deallocation = (u32)-1;
    // Bit i is set when page i of header_memory is committed. Covers the pages of both slab bitmaps.
    u64 committed_slab_bitmap_pages[(pool_constants::max_given_back_slab_bitmap_end + 4095) / 4096 / 64 + 1] = {};
    // Holds the pool object, the nonfull and given-back slab bitmaps, and the free and deallocation bitmaps.
    virtual_memory header_memory;
};

template<LockC Mutex>
struct pool
{
    pool_constants P;
    pool_state s;
    Mutex mutex;

    explicit pool(const pool_constants& P) noexcept;
};

template<u32 NumWords> u32 find_or_create_nonfull_slab(const pool_constants& P, pool_state& s);
template<u32 NumWords> void merge_deallocated_bits(const pool_constants& P, pool_state& s);
// Every buffer of slab must be free in its free bitmap.
template<u32 NumWords> void maybe_give_back_empty_slab(const pool_constants& P, pool_state& s, u32 slab);
// The caller holds the pool's mutex.
void give_back_empty_slabs(const pool_constants& P, pool_state& s);
void add_to_nonfull_slab_bitmap(const pool_constants& P, pool_state& s, u32 slab, u32 unit);
void remove_from_nonfull_slab_bitmap(const pool_constants& P, pool_state& s, u32 unit);
// With a null arena the pool holds no slab, and callers change s.num_allocated_buffers atomically.
template<LockC Mutex> pool<Mutex>* create_pool(const pool_constants& P, arena* arena);
// Every buffer of the pool must be deallocated.
template<LockC Mutex> void destroy_pool(pool<Mutex>* pool) noexcept;
// Returns a mapping of its own holding bytes at a multiple of alignment.
// Objects and arrays above Tunables.max_array_extent_bytes are large.
[[nodiscard]] void* allocate_large_buffer(usize bytes, usize alignment) noexcept;
void deallocate_large_buffer(void* p, usize bytes) noexcept;

constexpr inline u32 max_pool_per_type_indexes = 1 << 16;
template<typename T> constinit inline u32 pool_per_type_index = (u32)-1;
u32 find_or_assign_pool_per_type_index(u32& index) noexcept;

} // namespace floormat::slab::detail

namespace floormat::slab {

template<typename T, typename Resource> class std_allocator;

// TODO: convert between a buffer's address and its u32 index, so callers can store 4-byte references.
template<typename Tag, tunables Tunables = tunables{}>
class memory_resource final
{
    template<typename T, typename Resource> friend class std_allocator;

    static constexpr bool is_multi_threaded = Tunables.thread_model == thread_model::multi_threaded;
    static constexpr bool has_slabs = Tunables.buffer_source == buffer_source::slabs;
    template<typename T> static constexpr bool has_own_pool = Tunables.has_pool_per_type && !std::is_void_v<T>;
    using mutex_type = std::conditional_t<is_multi_threaded, Spinlock, detail::null_mutex>;
    using pool_type = detail::pool<mutex_type>;

    static constexpr u32 first_size_class_index(u32 alignment_shift);

    pool_type* find_pool(u32 index) const noexcept;
    template<usize Size, usize Align, typename T> pool_type* find_pool() const noexcept;
    pool_type* find_or_create_pool(u32 index, usize bytes, usize alignment) noexcept;
    template<usize Size, usize Align, typename T> pool_type* find_or_create_pool() noexcept;
    // Returns the pool pointers, reserving them at the first call.
    // Under multi_threaded, the caller holds _arena.lock.
    uptr* reserve_pool_pointers() noexcept;
    // Stored in arena::give_back_empty_slabs. Pools whose mutex is held keep their empty slabs.
    static void give_back_empty_slabs_of_pools(void* resource) noexcept;
    [[nodiscard]] void* allocate_without_slabs(usize bytes, usize alignment) noexcept;
    void deallocate_without_slabs(void* p, usize bytes, usize alignment) noexcept;

    detail::arena _arena{Tunables};
    // The address of an array holding a pool_type* per size_class_index(), then under Tunables.has_pool_per_type
    // one per pool_per_type_index value. Each is 0 before its pool's creation.
    uptr _pools = 0;
    virtual_memory _pool_pointer_memory;
    // Serves every request when Tunables.buffer_source is guard_pages.
    guard_page_memory_resource _guard_page_resource;
    u32 _num_committed_pool_per_type_pointers = 0;

public:
    static_assert(Tunables.arena_reservation_bytes <= u64{64} << 30 &&
                  Tunables.arena_reservation_bytes >= 1 << 20);
    static_assert((Tunables.arena_reservation_bytes & (Tunables.arena_reservation_bytes - 1)) == 0);
    static_assert(Tunables.max_slab_bytes >= 8192 && Tunables.max_slab_bytes <= 2 << 20 &&
                  Tunables.max_slab_bytes <= Tunables.arena_reservation_bytes);
    static_assert(Tunables.max_array_extent_bytes >= Tunables.max_slab_bytes &&
                  Tunables.max_array_extent_bytes <= 32 << 20 &&
                  Tunables.max_array_extent_bytes <= Tunables.arena_reservation_bytes);
    static_assert(Tunables.bytes_until_superpages == slab::tunables::no_superpages ||
                  Tunables.bytes_until_superpages % (64 << 10) == 0);
    static_assert(LockC<mutex_type>);

    static constexpr slab::tunables tunables = Tunables;
    // MSVC 19.53 rejects alignments above 8192 in alignas and __builtin_assume_aligned() (C2345).
    static constexpr usize max_alignment = 8192;
    static constexpr u32 num_size_classes = first_size_class_index(14);
    static constexpr u32 num_pool_pointers =
        num_size_classes + (Tunables.has_pool_per_type ? detail::max_pool_per_type_indexes : 0);

    // Returns the size class of size bytes at alignment, a power of two up to max_alignment,
    // or (u32)-1 when such a request gets a large buffer.
    static constexpr u32 size_class_index(usize size, usize alignment);
    // Returns size_class_index(detail::array_size_class_bytes(bytes, Align), Align).
    // The bytes must be at most detail::max_array_size_class_bytes.
    template<usize Align> static constexpr u32 array_size_class_index(usize bytes);
    // Rounds the size up to a multiple of the alignment, which is at least 16.
    template<usize Size, usize Align> static constexpr detail::pool_constants pool_constants{Size, Align, Tunables};

    constexpr memory_resource() noexcept = default;
    // Every buffer must be deallocated before destruction.
    ~memory_resource() noexcept;
    fm_DISABLE_MOVE_COPY(memory_resource);

    template<usize Size, usize Align, typename T = void>
    [[nodiscard]] void* allocate() noexcept requires (Align <= max_alignment);
    template<usize Size, usize Align, typename T = void>
    void deallocate(void* p) noexcept requires (Align <= max_alignment);
    template<usize Align> [[nodiscard]] void* allocate_array(usize bytes) noexcept
        requires (Align <= max_alignment);
    template<usize Align> void deallocate_array(void* p, usize bytes) noexcept
        requires (Align <= max_alignment);

    // Under single_allocating_thread, these must not run concurrently with allocate() or allocate_array().
    // Each is exact only while no deallocate() is running.
    template<usize Size, usize Align, typename T = void> u32 num_allocated_buffers() noexcept;
    template<usize Size, usize Align, typename T = void> u32 num_committed_slabs() noexcept requires (has_slabs);
    const detail::arena& arena() const noexcept;
    template<usize Size, usize Align, typename T = void> const detail::pool_state* pool_state() const noexcept;
};

template<typename Tag, tunables Tunables>
constexpr u32 memory_resource<Tag, Tunables>::first_size_class_index(u32 alignment_shift)
{
    u32 ret = 0;
    for (u32 shift = 4; shift < alignment_shift; shift++)
        ret += Tunables.max_slab_bytes >> shift;
    return ret;
}

template<typename Tag, tunables Tunables>
constexpr u32 memory_resource<Tag, Tunables>::size_class_index(usize size, usize alignment)
{
    fm_assert((alignment & (alignment - 1)) == 0 && alignment <= max_alignment);
    const usize align = alignment < 16 ? 16 : alignment;
    const usize bytes = ((size ? size : 1) + align - 1) / align * align;
    if (bytes > Tunables.max_slab_bytes)
        return (u32)-1;
    else
    {
        u32 shift = 4;
        while ((usize{1} << shift) < align)
            shift++;
        return first_size_class_index(shift) + (u32)(bytes / align) - 1;
    }
}

template<typename Tag, tunables Tunables>
template<usize Align>
constexpr u32 memory_resource<Tag, Tunables>::array_size_class_index(usize bytes)
{
    if constexpr (Align <= 16)
        return detail::array_size_class_indexes[(bytes + 15) / 16];
    else
        return size_class_index(detail::array_size_class_bytes(bytes, Align), Align);
}

template<typename T, typename Resource>
class allocator
{
    Resource* _resource;

public:
    explicit constexpr allocator(Resource* resource) noexcept;

    [[nodiscard]] void* allocate() const noexcept;
    void deallocate(void* p) const noexcept;

    template<typename... Xs>
    [[nodiscard]] T* construct(Xs&&... xs) const noexcept(std::is_nothrow_constructible_v<T, Xs...>);
    template<typename... Xs>
    [[nodiscard]] T* construct_with_braces(Xs&&... xs) const
        noexcept(noexcept(::new (std::declval<void*>()) T{std::declval<Xs>()...}));
    void destroy(T* p) const noexcept(std::is_nothrow_destructible_v<T>);

    // Same constraints as memory_resource's.
    u32 num_allocated_buffers() const noexcept;
    u32 num_committed_slabs() const noexcept;
    Resource* resource() const noexcept;
};

template<typename T, typename Resource>
constexpr allocator<T, Resource>::allocator(Resource* resource) noexcept: _resource{resource} {}

} // namespace floormat::slab
