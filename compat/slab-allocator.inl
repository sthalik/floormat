#pragma once
#include "slab-allocator.hpp"
#include "assert.hpp"
#include "assume-aligned.hpp"
#include "atomic.hpp"
#include "construct-at.hpp"
#include "move.hpp"
#include "start-lifetime-as.hpp"
#include <bit>
#include <new>
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace floormat::slab::detail {

inline void null_mutex::lock() noexcept {}
inline bool null_mutex::try_lock() noexcept { return true; }
inline void null_mutex::unlock() noexcept {}

inline void poison_memory_region([[maybe_unused]] const void* p, [[maybe_unused]] usize bytes) noexcept
{
#if fm_ASAN
    __asan_poison_memory_region(p, bytes);
#endif
}

inline void unpoison_memory_region([[maybe_unused]] const void* p, [[maybe_unused]] usize bytes) noexcept
{
#if fm_ASAN
    __asan_unpoison_memory_region(p, bytes);
#endif
}

// Returns the slab's bit in nonfull_slab_bitmap, given its first_buffer_offset.
constexpr u32 nonfull_slab_bitmap_index(u32 first_buffer_offset)
{
    constexpr u64 bytes_per_pagemap_entry = arena::bytes_per_pagemap_entry;
    return (u32)(((u64)first_buffer_offset * 16 + bytes_per_pagemap_entry - 1) / bytes_per_pagemap_entry);
}

template<u32 NumWords>
void push_slab_to_merge(pool_state& s, slab_deallocation_bitmap<NumWords>& d, u32 slab) noexcept
{
    u32 top = atomic_load(&s.first_slab_to_merge, memory_order::relaxed);
    do
        d.next_slab_to_merge = top;
    while (!atomic_compare_exchange_weak(&s.first_slab_to_merge, top, slab, memory_order::release));
}

template<u32 NumWords>
[[fm_always_inline]] inline char* allocate_buffer(const pool_constants& P, pool_state& s, u32 buffer_bytes,
                                                  bool is_deallocation_lock_free) noexcept
{
    u32 k = s.lowest_nonfull_slab;
    if (k == (u32)-1) [[unlikely]]
        k = find_or_create_nonfull_slab<NumWords>(P, s);
    auto& f = static_cast<slab_free_bitmap<NumWords>*>(s.free_bitmaps)[k];
    u32 w = 0;
    if constexpr (NumWords > 1)
        w = (u32)std::countr_zero(f.summary);
    const u64 word = f.words[w];
    const u32 i = w * 64 + (u32)std::countr_zero(word);
    char* const p = s.arena_base + (usize)f.first_buffer_offset * 16 + (usize)i * buffer_bytes;
    f.words[w] = word & (word - 1);
    if constexpr (NumWords > 1)
        f.num_free_buffers--;
    s.num_allocated_buffers++;
    if (!f.words[w]) [[unlikely]]
    {
        bool is_slab_used_up = true;
        if constexpr (NumWords > 1)
        {
            f.summary &= f.summary - 1;
            is_slab_used_up = !f.summary;
        }
        if (is_slab_used_up)
            remove_from_nonfull_slab_bitmap(P, s, s.lowest_nonfull_slab_unit);
        if (is_deallocation_lock_free)
            merge_deallocated_bits<NumWords>(P, s);
    }
    return p;
}

inline void set_needs_empty_slab_check(pool_state& s)
{
    if (!atomic_load(&s.needs_empty_slab_check, memory_order::relaxed))
    {
        atomic_store(&s.needs_empty_slab_check, u8{1}, memory_order::relaxed);
        // An exchange continues other pools' release sequences, so allocate_extent() sees their flags.
        (void)atomic_exchange(&s.arena->needs_empty_slab_check, u8{1}, memory_order::release);
    }
}

template<u32 NumWords>
[[fm_always_inline]] inline void deallocate_buffer(const pool_constants& P, pool_state& s, void* p,
                                                   u32 buffer_bytes, u32 buffer_index_multiplier, u32 slab_bytes,
                                                   u32 num_buffers_per_slab, bool is_lock_free,
                                                   bool is_slab_of_last_deallocation_cached,
                                                   bool is_empty_slab_given_back) noexcept
{
    constexpr u32 bytes_per_pagemap_entry = arena::bytes_per_pagemap_entry;
    constexpr u32 extent_start_mask = (1u << arena::num_extent_start_bits) - 1;
    is_slab_of_last_deallocation_cached = !is_lock_free && is_slab_of_last_deallocation_cached;
    auto load_value = [is_lock_free](const u32& x) {
        return is_lock_free ? atomic_load(&x, memory_order::relaxed) : x;
    };

    const usize offset = (usize)((uptr)p - (uptr)s.arena_base);
    fm_assert(offset < (usize)load_value(s.arena->bump_offset) * 16);
    auto* const F = static_cast<slab_free_bitmap<NumWords>*>(s.free_bitmaps);
    auto* const D = static_cast<slab_deallocation_bitmap<NumWords>*>(s.deallocation_bitmaps);
    u32 k = is_slab_of_last_deallocation_cached ? s.slab_of_last_deallocation : (u32)-1;
    u32 first_buffer_offset =
        is_slab_of_last_deallocation_cached ? s.first_buffer_offset_of_last_deallocation : (u32)-1;
    if (!is_slab_of_last_deallocation_cached || (u64)offset - (u64)first_buffer_offset * 16 >= slab_bytes)
    {
        const u32 unit = (u32)(offset / bytes_per_pagemap_entry);
        u32 entry = load_value(s.pagemap[unit]);
        if ((entry & extent_start_mask) && offset % bytes_per_pagemap_entry >= (entry & extent_start_mask) * 16)
            entry = load_value(s.pagemap[unit + 1]);
        k = entry >> arena::num_extent_start_bits;
        fm_assert(k < load_value(s.num_slabs_created));
        first_buffer_offset = is_lock_free ? D[k].first_buffer_offset : F[k].first_buffer_offset;
        if (is_slab_of_last_deallocation_cached)
        {
            s.slab_of_last_deallocation = k;
            s.first_buffer_offset_of_last_deallocation = first_buffer_offset;
        }
    }
    fm_debug3_assert(is_lock_free || first_buffer_offset == F[k].first_buffer_offset);

    // In u64, since a slab given back to the arena has first_buffer_offset (u32)-1.
    const u64 offset_in_slab = (u64)offset - (u64)first_buffer_offset * 16;
    fm_assert(offset_in_slab < slab_bytes);
    const u32 i = (u32)((offset_in_slab * buffer_index_multiplier) >> 32);
    fm_assert(i * buffer_bytes == (u32)offset_in_slab);
    const u32 w = i / 64;
    const u64 mask = u64{1} << (i % 64);

    poison_memory_region(p, buffer_bytes);
    if (!is_lock_free)
    {
        auto& f = F[k];
        const u64 old = f.words[w];
        fm_assert(!(old & mask));
        f.words[w] = old | mask;
        bool had_free_buffer = old != 0;
        if constexpr (NumWords > 1)
        {
            had_free_buffer = f.summary != 0;
            f.summary |= u64{1} << w;
        }
        if (!had_free_buffer)
            add_to_nonfull_slab_bitmap(P, s, k, nonfull_slab_bitmap_index(first_buffer_offset));
        s.num_allocated_buffers--;
        bool is_slab_empty;
        if constexpr (NumWords > 1)
            is_slab_empty = ++f.num_free_buffers == num_buffers_per_slab;
        else
            is_slab_empty = f.words[0] == ~u64{0} >> (64 - num_buffers_per_slab);
        if (is_slab_empty) [[unlikely]]
        {
            if (is_empty_slab_given_back)
                maybe_give_back_empty_slab<NumWords>(P, s, k);
            else
                set_needs_empty_slab_check(s);
        }
    }
    else
    {
        auto& d = D[k];
        const u64 old = atomic_fetch_or(&d.words[w], mask, memory_order::acq_rel);
        fm_assert(!(old & mask));
        bool needs_push = !old;
        if constexpr (NumWords > 1)
            if (!old)
                needs_push = !atomic_fetch_or(&d.summary, u64{1} << w, memory_order::acq_rel);
        if (needs_push)
            push_slab_to_merge(s, d, k);
    }
}

template<u32 NumWords, bool IsDeallocationLockFree, LockC Mutex>
[[fm_noinline]] char* allocate_buffer_in_pool(pool<Mutex>& pool, u32 buffer_bytes) noexcept
{
    pool.mutex.lock();
    char* const p = allocate_buffer<NumWords>(pool.P, pool.s, buffer_bytes, IsDeallocationLockFree);
    pool.mutex.unlock();
    return p;
}

template<u32 NumWords, bool IsDeallocationLockFree, bool IsSlabOfLastDeallocationCached, bool IsEmptySlabGivenBack>
[[fm_noinline]] void deallocate_buffer_in_pool(const pool_constants& P, pool_state& s, void* p,
                                               u32 buffer_bytes, u32 buffer_index_multiplier, u32 slab_bytes,
                                               u32 num_buffers_per_slab) noexcept
{
    deallocate_buffer<NumWords>(P, s, p, buffer_bytes, buffer_index_multiplier, slab_bytes, num_buffers_per_slab,
                                IsDeallocationLockFree, IsSlabOfLastDeallocationCached, IsEmptySlabGivenBack);
}

template<typename F>
decltype(auto) call_with_num_bitmap_words(u32 num_words, F&& f)
{
    switch (num_words)
    {
    case 1: return f(std::integral_constant<u32, 1>{});
    case 2: return f(std::integral_constant<u32, 2>{});
    case 4: return f(std::integral_constant<u32, 4>{});
    case 8: return f(std::integral_constant<u32, 8>{});
    case 16: return f(std::integral_constant<u32, 16>{});
    case 32: return f(std::integral_constant<u32, 32>{});
    case 64: return f(std::integral_constant<u32, 64>{});
    default: fm_abort("slab::memory_resource: %u bitmap words per slab", num_words);
    }
}

} // namespace floormat::slab::detail

namespace floormat::slab {

template<typename Tag, tunables Tunables>
memory_resource<Tag, Tunables>::~memory_resource() noexcept
{
    fm_assert(_arena.num_allocated_array_extents == 0);
    auto* const table = reinterpret_cast<uptr*>(_pools);
    if (table)
        for (u32 i = 0; i < num_size_classes + _num_committed_pool_per_type_pointers; i++)
            if (table[i])
                detail::destroy_pool(reinterpret_cast<pool_type*>(table[i]));
}

template<typename Tag, tunables Tunables>
auto memory_resource<Tag, Tunables>::find_pool(u32 index) const noexcept -> pool_type*
{
    if constexpr (is_multi_threaded)
    {
        const uptr table = atomic_load(&_pools, memory_order::acquire);
        if (!table)
            return nullptr;
        else
            return reinterpret_cast<pool_type*>(atomic_load(reinterpret_cast<uptr*>(table) + index,
                                                            memory_order::acquire));
    }
    else if (!_pools)
        return nullptr;
    else
        return reinterpret_cast<pool_type*>(reinterpret_cast<uptr*>(_pools)[index]);
}

template<typename Tag, tunables Tunables>
template<usize Size, usize Align, typename T>
auto memory_resource<Tag, Tunables>::find_pool() const noexcept -> pool_type*
{
    if constexpr (has_own_pool<T>)
    {
        const u32 i = atomic_load(&detail::pool_per_type_index<T>, memory_order::relaxed);
        u32 num_committed;
        if constexpr (Tunables.thread_model == thread_model::single_threaded)
            num_committed = _num_committed_pool_per_type_pointers;
        else
            num_committed = atomic_load(&_num_committed_pool_per_type_pointers,
                                        is_multi_threaded ? memory_order::acquire : memory_order::relaxed);
        if (i >= num_committed)
            return nullptr;
        else
            return find_pool(num_size_classes + i);
    }
    else
    {
        constexpr u32 index = size_class_index(Size, Align);
        return find_pool(index);
    }
}

template<typename Tag, tunables Tunables>
void memory_resource<Tag, Tunables>::give_back_empty_slabs_of_pools(void* resource) noexcept
{
    auto& r = *static_cast<memory_resource*>(resource);
    u32 num_pools = num_size_classes;
    if constexpr (Tunables.has_pool_per_type)
    {
        if constexpr (Tunables.thread_model == thread_model::single_threaded)
            num_pools += r._num_committed_pool_per_type_pointers;
        else
            num_pools += atomic_load(&r._num_committed_pool_per_type_pointers, memory_order::acquire);
    }
    for (u32 i = 0; i < num_pools; i++)
    {
        pool_type* const pool = r.find_pool(i);
        if (!pool || !atomic_load(&pool->s.needs_empty_slab_check, memory_order::relaxed))
            continue;
        else if (!pool->mutex.try_lock())
            (void)atomic_exchange(&r._arena.needs_empty_slab_check, u8{1}, memory_order::release);
        else
        {
            atomic_store(&pool->s.needs_empty_slab_check, u8{0}, memory_order::relaxed);
            detail::give_back_empty_slabs(pool->P, pool->s);
            pool->mutex.unlock();
        }
    }
}

template<typename Tag, tunables Tunables>
template<usize Size, usize Align, typename T>
auto memory_resource<Tag, Tunables>::find_or_create_pool() noexcept -> pool_type*
{
    if constexpr (has_own_pool<T>)
    {
        const u32 i = detail::find_or_assign_pool_per_type_index(detail::pool_per_type_index<T>);
        return find_or_create_pool(num_size_classes + i, Size, Align);
    }
    else
        return find_or_create_pool(size_class_index(Size, Align), Size, Align);
}

template<typename Tag, tunables Tunables>
uptr* memory_resource<Tag, Tunables>::reserve_pool_pointers() noexcept
{
    if (!_pools)
    {
        // Committed whole, since find_pool() reads the entries of pools not created yet.
        constexpr usize bytes = num_size_classes * sizeof(uptr);
        _pool_pointer_memory.reserve(num_pool_pointers * sizeof(uptr));
        _pool_pointer_memory.commit(0, bytes);
        uptr* const table = start_lifetime_as_array<uptr>(_pool_pointer_memory.data(), num_size_classes);
        if constexpr (is_multi_threaded)
            atomic_store(&_pools, (uptr)table, memory_order::release);
        else
            _pools = (uptr)table;
    }
    return reinterpret_cast<uptr*>(_pools);
}

template<typename Tag, tunables Tunables>
[[fm_noinline]]
auto memory_resource<Tag, Tunables>::find_or_create_pool(u32 index, usize bytes, usize alignment) noexcept
    -> pool_type*
{
    const detail::pool_constants P{bytes, alignment, Tunables};
    if constexpr (is_multi_threaded)
        _arena.lock.lock();
    uptr* const table = reserve_pool_pointers();
    if constexpr (Tunables.has_pool_per_type)
    {
        const u32 num_committed = _num_committed_pool_per_type_pointers;
        if (index >= num_size_classes + num_committed)
        {
            constexpr usize page_bytes = 4096, max_end = usize{num_pool_pointers} * sizeof(uptr);
            const usize first = usize{num_size_classes + num_committed} * sizeof(uptr);
            const usize end_of_page = (((usize)index + 1) * sizeof(uptr) + page_bytes - 1) / page_bytes * page_bytes;
            const usize end = end_of_page < max_end ? end_of_page : max_end;
            const u32 new_num_committed = (u32)(end / sizeof(uptr)) - num_size_classes;
            _pool_pointer_memory.commit(first, end - first);
            start_lifetime_as_array<uptr>(_pool_pointer_memory.data() + first, new_num_committed - num_committed);
            if constexpr (Tunables.thread_model == thread_model::single_threaded)
                _num_committed_pool_per_type_pointers = new_num_committed;
            else
                atomic_store(&_num_committed_pool_per_type_pointers, new_num_committed, memory_order::release);
        }
    }
    auto* pool = reinterpret_cast<pool_type*>(table[index]);
    if (!pool)
    {
        if constexpr (!has_slabs)
            pool = detail::create_pool<mutex_type>(P, nullptr);
        else
        {
            if (!_arena.base)
                detail::reserve_arena_addr_range(_arena);
            _arena.give_back_empty_slabs = &give_back_empty_slabs_of_pools;
            _arena.resource = this;
            pool = detail::create_pool<mutex_type>(P, &_arena);
        }
        if constexpr (is_multi_threaded)
            atomic_store(&table[index], (uptr)pool, memory_order::release);
        else
            table[index] = (uptr)pool;
    }
    if constexpr (is_multi_threaded)
        _arena.lock.unlock();
    return pool;
}

template<typename Tag, tunables Tunables>
void* memory_resource<Tag, Tunables>::allocate_without_slabs(usize bytes, usize alignment) noexcept
{
    if constexpr (Tunables.buffer_source == buffer_source::guard_pages)
        return _guard_page_resource.allocate(bytes, alignment);
    else
    {
        void* const p = alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__
                        ? ::operator new(bytes, std::align_val_t{alignment}, std::nothrow)
                        : ::operator new(bytes, std::nothrow);
        if (!p) [[unlikely]]
            fm_abort("slab::memory_resource: operator new of %zu bytes at alignment %zu failed", bytes, alignment);
        return p;
    }
}

template<typename Tag, tunables Tunables>
void memory_resource<Tag, Tunables>::deallocate_without_slabs(void* p, usize bytes, usize alignment) noexcept
{
    if constexpr (Tunables.buffer_source == buffer_source::guard_pages)
        _guard_page_resource.deallocate(p, bytes, alignment);
#ifdef __cpp_sized_deallocation
    else if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        ::operator delete(p, bytes, std::align_val_t{alignment});
    else
        ::operator delete(p, bytes);
#else
    else if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        ::operator delete(p, std::align_val_t{alignment});
    else
        ::operator delete(p);
#endif
}

template<typename Tag, tunables Tunables>
template<usize Size, usize Align, typename T>
[[fm_always_inline]] inline void* memory_resource<Tag, Tunables>::allocate() noexcept
    requires (Align <= max_alignment)
{
    if constexpr (!std::is_void_v<T>)
        static_assert(Size == sizeof(T) && Align == alignof(T));
    constexpr u32 index = size_class_index(Size, Align);
    if constexpr (!std::is_void_v<T> && !has_own_pool<T>)
        return allocate<Size, Align, void>();
    else if constexpr (index == (u32)-1)
    {
        if constexpr (!has_slabs)
            return assume_aligned<Align>(allocate_without_slabs(Size, Align < 16 ? 16 : Align));
        else
            return assume_aligned<Align>(allocate_array<Align>(Size));
    }
    else
    {
        constexpr auto& P = pool_constants<Size, Align>;
        pool_type* pool = find_pool<Size, Align, T>();
        if (!pool) [[unlikely]]
            pool = find_or_create_pool<Size, Align, T>();
        if constexpr (!has_slabs)
        {
            // P.buffer_align keeps the 16-byte alignment that slabs give.
            void* const p = allocate_without_slabs(Size, P.buffer_align);
            atomic_fetch_add(&pool->s.num_allocated_buffers, 1u, memory_order::relaxed);
            return assume_aligned<P.buffer_align>(p);
        }
        else
        {
            pool->mutex.lock();
            char* const p = detail::allocate_buffer<P.num_bitmap_words_per_slab>(P, pool->s, P.buffer_bytes,
                                                                                 P.is_deallocation_lock_free());
            pool->mutex.unlock();
            detail::unpoison_memory_region(p, Size);
            return assume_aligned<P.buffer_align>(p);
        }
    }
}

template<typename Tag, tunables Tunables>
template<usize Size, usize Align, typename T>
[[fm_always_inline]] inline void memory_resource<Tag, Tunables>::deallocate(void* p) noexcept
    requires (Align <= max_alignment)
{
    if constexpr (!std::is_void_v<T>)
        static_assert(Size == sizeof(T) && Align == alignof(T));
    constexpr u32 index = size_class_index(Size, Align);
    if constexpr (!std::is_void_v<T> && !has_own_pool<T>)
        deallocate<Size, Align, void>(p);
    else if constexpr (index == (u32)-1)
    {
        if constexpr (!has_slabs)
            deallocate_without_slabs(p, Size, Align < 16 ? 16 : Align);
        else
            deallocate_array<Align>(p, Size);
    }
    else
    {
        constexpr auto& P = pool_constants<Size, Align>;
        pool_type* const pool = find_pool<Size, Align, T>();
        fm_assert(pool);
        if constexpr (!has_slabs)
        {
            deallocate_without_slabs(p, Size, P.buffer_align);
            atomic_fetch_sub(&pool->s.num_allocated_buffers, 1u, memory_order::relaxed);
        }
        else
            detail::deallocate_buffer<P.num_bitmap_words_per_slab>(
                P, pool->s, p, P.buffer_bytes, P.buffer_index_multiplier, P.slab_bytes, P.num_buffers_per_slab,
                P.is_deallocation_lock_free(), P.is_slab_of_last_deallocation_cached, P.is_empty_slab_given_back);
    }
}

template<typename Tag, tunables Tunables>
template<usize Align>
[[fm_noinline]] void* memory_resource<Tag, Tunables>::allocate_array(usize bytes) noexcept
    requires (Align <= max_alignment)
{
    constexpr usize align = Align < 16 ? 16 : Align;
    constexpr bool is_deallocation_lock_free = Tunables.thread_model != thread_model::single_threaded;
    if (bytes <= detail::max_array_size_class_bytes)
    {
        const u32 index = array_size_class_index<Align>(bytes);
        pool_type* pool = find_pool(index);
        if (!pool) [[unlikely]]
            pool = find_or_create_pool(index, Align <= 16 ? ((usize)index + 1) * 16
                                                          : detail::array_size_class_bytes(bytes, Align), Align);
        if constexpr (!has_slabs)
        {
            void* const p = allocate_without_slabs(bytes, align);
            atomic_fetch_add(&pool->s.num_allocated_buffers, 1u, memory_order::relaxed);
            return assume_aligned<align>(p);
        }
        else
        {
            char* const p = detail::call_with_num_bitmap_words(
                pool->P.num_bitmap_words_per_slab, [pool](auto num_words) {
                    return detail::allocate_buffer_in_pool<decltype(num_words)::value, is_deallocation_lock_free,
                                                           mutex_type>(*pool, pool->P.buffer_bytes);
                });
            detail::unpoison_memory_region(p, bytes);
            return assume_aligned<align>(p);
        }
    }
    else if constexpr (!has_slabs)
        return assume_aligned<align>(allocate_without_slabs(bytes, align));
    else if (bytes > Tunables.max_array_extent_bytes / 16 * 16)
        return assume_aligned<Align>(detail::allocate_large_buffer(bytes, Align));
    else
    {
        constexpr u32 bytes_per_pagemap_entry = detail::arena::bytes_per_pagemap_entry;
        const u32 rounded_bytes = (u32)(bytes + 15) / 16 * 16;
        const u32 extent_bytes = rounded_bytes < bytes_per_pagemap_entry ? bytes_per_pagemap_entry : rounded_bytes;
        const u32 first = detail::allocate_extent(_arena, extent_bytes, (u32)align,
                                                  detail::arena::array_extent_marker);
        char* const p = _arena.base + (usize)first * 16;
        detail::poison_memory_region(p, extent_bytes);
        detail::unpoison_memory_region(p, bytes);
        return assume_aligned<align>(p);
    }
}

template<typename Tag, tunables Tunables>
template<usize Align>
[[fm_noinline]] void memory_resource<Tag, Tunables>::deallocate_array(void* p, usize bytes) noexcept
    requires (Align <= max_alignment)
{
    constexpr usize align = Align < 16 ? 16 : Align;
    constexpr bool is_deallocation_lock_free = Tunables.thread_model != thread_model::single_threaded;
    constexpr bool is_slab_of_last_deallocation_cached =
        !is_deallocation_lock_free && Tunables.is_slab_of_last_deallocation_cached;
    if (bytes <= detail::max_array_size_class_bytes)
    {
        pool_type* const pool = find_pool(array_size_class_index<Align>(bytes));
        fm_assert(pool);
        if constexpr (!has_slabs)
        {
            deallocate_without_slabs(p, bytes, align);
            atomic_fetch_sub(&pool->s.num_allocated_buffers, 1u, memory_order::relaxed);
        }
        else
        {
            const detail::pool_constants& P = pool->P;
            detail::call_with_num_bitmap_words(P.num_bitmap_words_per_slab, [&](auto num_words) {
                detail::deallocate_buffer_in_pool<decltype(num_words)::value, is_deallocation_lock_free,
                                                  is_slab_of_last_deallocation_cached,
                                                  Tunables.is_empty_slab_given_back>(
                    P, pool->s, p, P.buffer_bytes, P.buffer_index_multiplier, P.slab_bytes, P.num_buffers_per_slab);
            });
        }
    }
    else if constexpr (!has_slabs)
        deallocate_without_slabs(p, bytes, align);
    else if (bytes > Tunables.max_array_extent_bytes / 16 * 16)
        detail::deallocate_large_buffer(p, bytes);
    else
    {
        constexpr u32 bytes_per_pagemap_entry = detail::arena::bytes_per_pagemap_entry;
        const u32 rounded_bytes = (u32)(bytes + 15) / 16 * 16;
        const u32 extent_bytes = rounded_bytes < bytes_per_pagemap_entry ? bytes_per_pagemap_entry : rounded_bytes;
        const usize offset = (usize)((uptr)p - (uptr)_arena.base);
        fm_assert(offset % 16 == 0 && offset / 16 < _arena.max_bump_offset);
        if constexpr (Tunables.thread_model == thread_model::single_threaded)
            detail::deallocate_extent(_arena, (u32)(offset / 16), extent_bytes,
                                      detail::arena::array_extent_marker);
        else
            detail::push_pending_array_extent(_arena, (u32)(offset / 16), extent_bytes);
    }
}

template<typename Tag, tunables Tunables>
template<usize Size, usize Align, typename T>
u32 memory_resource<Tag, Tunables>::num_allocated_buffers() noexcept
{
    constexpr u32 index = size_class_index(Size, Align);
    static_assert(index != (u32)-1);
    constexpr auto& P = pool_constants<Size, Align>;
    pool_type* const pool = find_pool<Size, Align, T>();
    if (!pool)
        return 0;
    else if (!has_slabs)
        return atomic_load(&pool->s.num_allocated_buffers, memory_order::relaxed);
    else
    {
        pool->mutex.lock();
        if constexpr (P.is_deallocation_lock_free())
            detail::merge_deallocated_bits<P.num_bitmap_words_per_slab>(P, pool->s);
        const u32 ret = pool->s.num_allocated_buffers;
        pool->mutex.unlock();
        return ret;
    }
}

template<typename Tag, tunables Tunables>
template<usize Size, usize Align, typename T>
u32 memory_resource<Tag, Tunables>::num_committed_slabs() noexcept requires (has_slabs)
{
    constexpr u32 index = size_class_index(Size, Align);
    static_assert(index != (u32)-1);
    constexpr auto& P = pool_constants<Size, Align>;
    pool_type* const pool = find_pool<Size, Align, T>();
    if (!pool)
        return 0;
    else
    {
        pool->mutex.lock();
        if constexpr (P.is_deallocation_lock_free())
            detail::merge_deallocated_bits<P.num_bitmap_words_per_slab>(P, pool->s);
        const u32 ret = pool->s.num_slabs_created - pool->s.num_given_back_slabs;
        pool->mutex.unlock();
        return ret;
    }
}

template<typename Tag, tunables Tunables>
const detail::arena& memory_resource<Tag, Tunables>::arena() const noexcept
{
    return _arena;
}

template<typename Tag, tunables Tunables>
template<usize Size, usize Align, typename T>
const detail::pool_state* memory_resource<Tag, Tunables>::pool_state() const noexcept
{
    constexpr u32 index = size_class_index(Size, Align);
    static_assert(index != (u32)-1);
    const pool_type* const pool = find_pool<Size, Align, T>();
    return pool ? &pool->s : nullptr;
}

template<typename T, typename Resource>
void* allocator<T, Resource>::allocate() const noexcept
{
    return _resource->template allocate<sizeof(T), alignof(T), T>();
}

template<typename T, typename Resource>
void allocator<T, Resource>::deallocate(void* p) const noexcept
{
    _resource->template deallocate<sizeof(T), alignof(T), T>(p);
}

template<typename T, typename Resource>
template<typename... Xs>
T* allocator<T, Resource>::construct(Xs&&... xs) const noexcept(std::is_nothrow_constructible_v<T, Xs...>)
{
    void* const p = allocate();
    T* ret;
    if constexpr (std::is_nothrow_constructible_v<T, Xs...>)
        ret = construct_at(static_cast<T*>(p), forward<Xs>(xs)...);
    else
        try {
            ret = construct_at(static_cast<T*>(p), forward<Xs>(xs)...);
        } catch (...) {
            deallocate(p);
            throw;
        }
    return ret;
}

template<typename T, typename Resource>
template<typename... Xs>
T* allocator<T, Resource>::construct_with_braces(Xs&&... xs) const
    noexcept(noexcept(::new (std::declval<void*>()) T{std::declval<Xs>()...}))
{
    void* const p = allocate();
    T* ret;
    if constexpr (noexcept(::new (std::declval<void*>()) T{std::declval<Xs>()...}))
        ret = ::new (p) T{forward<Xs>(xs)...};
    else
        try {
            ret = ::new (p) T{forward<Xs>(xs)...};
        } catch (...) {
            deallocate(p);
            throw;
        }
    return ret;
}

template<typename T, typename Resource>
void allocator<T, Resource>::destroy(T* p) const noexcept(std::is_nothrow_destructible_v<T>)
{
    if constexpr (std::is_nothrow_destructible_v<T>)
        destroy_at(p);
    else
        try {
            destroy_at(p);
        } catch (...) {
            deallocate(p);
            throw;
        }
    deallocate(p);
}

template<typename T, typename Resource>
u32 allocator<T, Resource>::num_allocated_buffers() const noexcept
{
    return _resource->template num_allocated_buffers<sizeof(T), alignof(T), T>();
}

template<typename T, typename Resource>
u32 allocator<T, Resource>::num_committed_slabs() const noexcept
{
    return _resource->template num_committed_slabs<sizeof(T), alignof(T), T>();
}

template<typename T, typename Resource>
Resource* allocator<T, Resource>::resource() const noexcept
{
    return _resource;
}

} // namespace floormat::slab
