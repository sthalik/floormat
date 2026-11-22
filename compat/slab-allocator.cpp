#include "slab-allocator.inl"
#include "start-lifetime-as.hpp"
#include "virtual-memory.hpp"
#include <bit>
#include <new>

namespace floormat::slab::detail {

namespace {

constexpr u32 page_bytes = 4096;

constinit u32 num_pool_per_type_indexes = 0;

usize round_up(usize x, usize multiple) { return (x + multiple - 1) / multiple * multiple; }

u64 mask_trailing_ones(u32 n) { return n ? ~u64{0} >> (64 - n) : 0; }

// Commits the pages holding slab's bitmap in the header array at region_offset.
// The bitmaps below it must be committed.
void commit_pages_holding_bitmap(pool_state& s, u32 region_offset, u32 element_bytes, u32 slab)
{
    const usize begin = round_up((usize)region_offset + (usize)slab * element_bytes, page_bytes);
    const usize end = round_up((usize)region_offset + (usize)(slab + 1) * element_bytes, page_bytes);
    if (begin < end)
        s.header_memory.commit(begin, end - begin);
}

void commit_slab_bitmap_page(pool_state& s, const u64* word)
{
    const u32 page = (u32)((uptr)word - (uptr)s.header_memory.data()) / page_bytes;
    u64& committed = s.committed_slab_bitmap_pages[page / 64];
    const u64 mask = u64{1} << (page % 64);
    if (!(committed & mask))
    {
        s.header_memory.commit((usize)page * page_bytes, page_bytes);
        (void)start_lifetime_as_array<u64>(s.header_memory.data() + (usize)page * page_bytes, page_bytes / 8);
        committed |= mask;
    }
}

// The levels must lie in the pages that committed_slab_bitmap_pages tracks.
void set_bit_with_summaries(pool_state& s, u64* const* levels, u32 num_levels, u32 i)
{
    for (u32 level = 0; level < num_levels; level++, i /= 64)
    {
        u64* const word = &levels[level][i / 64];
        commit_slab_bitmap_page(s, word);
        const u64 old = *word;
        *word = old | u64{1} << (i % 64);
        if (old)
            break;
    }
}

void clear_bit_with_summaries(u64* const* levels, u32 num_levels, u32 i)
{
    for (u32 level = 0; level < num_levels; level++, i /= 64)
    {
        u64& word = levels[level][i / 64];
        word &= ~(u64{1} << (i % 64));
        if (word)
            break;
    }
}

u32 find_lowest_set_bit(u64* const* levels, u32 num_levels)
{
    if (!levels[num_levels - 1][0])
        return (u32)-1;
    else
    {
        u32 i = 0;
        for (u32 level = num_levels; level-- > 0; )
            i = i * 64 + (u32)std::countr_zero(levels[level][i]);
        return i;
    }
}

template<u32 NumWords>
bool is_slab_empty(const pool_constants& P, const slab_free_bitmap<NumWords>& f)
{
    if constexpr (NumWords > 1)
        return f.num_free_buffers == P.num_buffers_per_slab;
    else
        return f.words[0] == mask_trailing_ones(P.num_buffers_per_slab);
}

template<u32 NumWords>
void give_back_slab(const pool_constants& P, pool_state& s, u32 slab)
{
    auto& f = static_cast<slab_free_bitmap<NumWords>*>(s.free_bitmaps)[slab];
    remove_from_nonfull_slab_bitmap(P, s, nonfull_slab_bitmap_index(f.first_buffer_offset));
    deallocate_extent(*s.arena, f.first_buffer_offset, P.slab_bytes, slab);
    f.first_buffer_offset = (u32)-1;
    if (s.slab_of_last_deallocation == slab)
        s.first_buffer_offset_of_last_deallocation = (u32)-1;
    if (s.kept_empty_slab == slab)
        s.kept_empty_slab = (u32)-1;
    auto* const D = static_cast<slab_deallocation_bitmap<NumWords>*>(s.deallocation_bitmaps);
    if (P.is_deallocation_lock_free())
        D[slab].first_buffer_offset = (u32)-1;
    set_bit_with_summaries(s, s.given_back_slab_bitmap, P.num_given_back_slab_bitmap_levels, slab);
    s.num_given_back_slabs++;
}

template<u32 NumWords>
void create_slab(const pool_constants& P, pool_state& s)
{
    // No slab has a free buffer, so none is empty.
    atomic_store(&s.needs_empty_slab_check, u8{0}, memory_order::relaxed);
    u32 k;
    const bool all_slab_indexes_in_use = !s.num_given_back_slabs;
    if (!all_slab_indexes_in_use)
    {
        k = find_lowest_set_bit(s.given_back_slab_bitmap, P.num_given_back_slab_bitmap_levels);
        clear_bit_with_summaries(s.given_back_slab_bitmap, P.num_given_back_slab_bitmap_levels, k);
        s.num_given_back_slabs--;
    }
    else
    {
        k = s.num_slabs_created;
        fm_assert(k < P.max_slabs);
        commit_pages_holding_bitmap(s, P.free_bitmaps_offset, P.free_bitmap_bytes, k);
        if (P.is_deallocation_lock_free())
            commit_pages_holding_bitmap(s, P.deallocation_bitmaps_offset, P.deallocation_bitmap_bytes, k);
    }
    // Buffers get at least the alignment they would have in a slab starting a cache line.
    constexpr u32 line_bytes = (u32)hardware_constructive_interference_size;
    const u32 buffer_bytes_alignment = u32{1} << std::countr_zero(P.buffer_bytes);
    u32 alignment = buffer_bytes_alignment < line_bytes ? buffer_bytes_alignment : line_bytes;
    if (P.buffer_align > alignment)
        alignment = P.buffer_align;
    const u32 first = allocate_extent(*s.arena, P.slab_bytes, alignment, k);
    poison_memory_region(s.arena_base + (usize)first * 16, P.slab_bytes);

    auto* const f = construct_at(static_cast<slab_free_bitmap<NumWords>*>(s.free_bitmaps) + k);
    f->first_buffer_offset = first;
    const u32 b = P.num_buffers_per_slab;
    for (u32 w = 0; w < NumWords; w++)
        f->words[w] = b >= (w + 1) * 64 ? ~u64{0} : b > w * 64 ? mask_trailing_ones(b - w * 64) : 0;
    if constexpr (NumWords > 1)
    {
        f->summary = mask_trailing_ones((b + 63) / 64);
        f->num_free_buffers = b;
    }
    if (P.is_deallocation_lock_free())
    {
        auto* const d = construct_at(static_cast<slab_deallocation_bitmap<NumWords>*>(s.deallocation_bitmaps) + k);
        d->first_buffer_offset = first;
        d->next_slab_to_merge = (u32)-1;
        if (all_slab_indexes_in_use)
            atomic_store(&s.num_slabs_created, k + 1, memory_order::relaxed);
    }
    else if (all_slab_indexes_in_use)
        s.num_slabs_created = k + 1;
    add_to_nonfull_slab_bitmap(P, s, k, nonfull_slab_bitmap_index(first));
}

} // namespace

template<u32 NumWords>
u32 find_or_create_nonfull_slab(const pool_constants& P, pool_state& s)
{
    if (P.is_deallocation_lock_free())
        merge_deallocated_bits<NumWords>(P, s);
    if (s.lowest_nonfull_slab == (u32)-1)
        create_slab<NumWords>(P, s);
    return s.lowest_nonfull_slab;
}

template<u32 NumWords>
void merge_deallocated_bits(const pool_constants& P, pool_state& s)
{
    if (atomic_load(&s.first_slab_to_merge, memory_order::relaxed) != (u32)-1)
    {
        auto* const F = static_cast<slab_free_bitmap<NumWords>*>(s.free_bitmaps);
        auto* const D = static_cast<slab_deallocation_bitmap<NumWords>*>(s.deallocation_bitmaps);
        u32 k = atomic_exchange(&s.first_slab_to_merge, (u32)-1, memory_order::acquire);
        while (k != (u32)-1)
        {
            auto& f = F[k];
            auto& d = D[k];
            fm_assert(f.first_buffer_offset != (u32)-1);
            // Read before the exchange, since a deallocate() that then finds it zero pushes the slab again.
            const u32 next = d.next_slab_to_merge;
            u32 num_bits = 0;
            bool had_free_buffer;
            if constexpr (NumWords > 1)
            {
                had_free_buffer = f.summary != 0;
                u64 summary = atomic_exchange(&d.summary, u64{0}, memory_order::acq_rel);
                while (summary)
                {
                    const u32 w = (u32)std::countr_zero(summary);
                    summary &= summary - 1;
                    const u64 bits = atomic_exchange(&d.words[w], u64{0}, memory_order::acq_rel);
                    fm_assert(!(f.words[w] & bits));
                    if (bits)
                    {
                        f.words[w] |= bits;
                        f.summary |= u64{1} << w;
                        num_bits += (u32)std::popcount(bits);
                    }
                }
            }
            else
            {
                had_free_buffer = f.words[0] != 0;
                const u64 bits = atomic_exchange(&d.words[0], u64{0}, memory_order::acq_rel);
                fm_assert(!(f.words[0] & bits));
                f.words[0] |= bits;
                num_bits = (u32)std::popcount(bits);
            }
            s.num_allocated_buffers -= num_bits;
            if constexpr (NumWords > 1)
                f.num_free_buffers += num_bits;
            if (!had_free_buffer && num_bits)
                add_to_nonfull_slab_bitmap(P, s, k, nonfull_slab_bitmap_index(f.first_buffer_offset));
            if (num_bits && is_slab_empty<NumWords>(P, f))
            {
                if (P.is_empty_slab_given_back)
                    maybe_give_back_empty_slab<NumWords>(P, s, k);
                else
                    set_needs_empty_slab_check(s);
            }
            k = next;
        }
    }
}

[[fm_noinline]] void add_to_nonfull_slab_bitmap(const pool_constants& P, pool_state& s, u32 slab, u32 unit)
{
    set_bit_with_summaries(s, s.nonfull_slab_bitmap, P.num_nonfull_slab_bitmap_levels, unit);
    if (s.lowest_nonfull_slab == (u32)-1 || unit < s.lowest_nonfull_slab_unit)
    {
        s.lowest_nonfull_slab = slab;
        s.lowest_nonfull_slab_unit = unit;
    }
}

[[fm_noinline]] void remove_from_nonfull_slab_bitmap(const pool_constants& P, pool_state& s, u32 unit)
{
    clear_bit_with_summaries(s.nonfull_slab_bitmap, P.num_nonfull_slab_bitmap_levels, unit);
    if (unit == s.lowest_nonfull_slab_unit)
    {
        const u32 lowest = find_lowest_set_bit(s.nonfull_slab_bitmap, P.num_nonfull_slab_bitmap_levels);
        if (lowest == (u32)-1)
            s.lowest_nonfull_slab = (u32)-1;
        else
        {
            const u32 entry = P.is_deallocation_lock_free() ? atomic_load(&s.pagemap[lowest], memory_order::relaxed)
                                                            : s.pagemap[lowest];
            s.lowest_nonfull_slab = entry >> arena::num_extent_start_bits;
            s.lowest_nonfull_slab_unit = lowest;
        }
    }
}

template<u32 NumWords>
void maybe_give_back_empty_slab(const pool_constants& P, pool_state& s, u32 slab)
{
    auto* const F = static_cast<slab_free_bitmap<NumWords>*>(s.free_bitmaps);
    const u32 kept = s.kept_empty_slab;
    if (kept == (u32)-1 || kept == slab || !is_slab_empty<NumWords>(P, F[kept]))
        s.kept_empty_slab = slab;
    else
        give_back_slab<NumWords>(P, s, slab);
}

void give_back_empty_slabs(const pool_constants& P, pool_state& s)
{
    call_with_num_bitmap_words(P.num_bitmap_words_per_slab, [&](auto num_words) {
        constexpr u32 NumWords = decltype(num_words)::value;
        const auto* const F = static_cast<slab_free_bitmap<NumWords>*>(s.free_bitmaps);
        const u32 num_slabs = atomic_load(&s.num_slabs_created, memory_order::relaxed);
        for (u32 k = 0; k < num_slabs; k++)
            if (F[k].first_buffer_offset != (u32)-1 && is_slab_empty<NumWords>(P, F[k]))
                give_back_slab<NumWords>(P, s, k);
    });
}

template<LockC Mutex>
pool<Mutex>::pool(const pool_constants& P) noexcept: P{P} {}

template<LockC Mutex>
pool<Mutex>* create_pool(const pool_constants& P, arena* arena)
{
    static_assert(sizeof(pool<Mutex>) <= pool_constants::pool_object_bytes);
    virtual_memory header_memory;
    header_memory.reserve(arena ? P.header_reservation_bytes : pool_constants::pool_object_bytes);
    header_memory.commit(0, page_bytes);
    char* const base = header_memory.data();
    auto* const pool = construct_at(reinterpret_cast<detail::pool<Mutex>*>(base), P);
    pool_state& s = pool->s;
    if (arena)
    {
        s.arena = arena;
        s.arena_base = arena->base;
        s.pagemap = arena->pagemap;
        (void)start_lifetime_as_array<u64>(base + pool_constants::pool_object_bytes,
                                           (page_bytes - pool_constants::pool_object_bytes) / 8);
        for (u32 level = 0; level < P.num_nonfull_slab_bitmap_levels; level++)
            s.nonfull_slab_bitmap[level] = reinterpret_cast<u64*>(base) + P.nonfull_slab_bitmap_level_offset[level];
        for (u32 level = 0; level < P.num_given_back_slab_bitmap_levels; level++)
            s.given_back_slab_bitmap[level] =
                reinterpret_cast<u64*>(base) + P.given_back_slab_bitmap_level_offset[level];
        s.committed_slab_bitmap_pages[0] = 1;
        s.free_bitmaps = base + P.free_bitmaps_offset;
        if (P.is_deallocation_lock_free())
            s.deallocation_bitmaps = base + P.deallocation_bitmaps_offset;
    }
    s.header_memory = move(header_memory);
    return pool;
}

template<LockC Mutex>
void destroy_pool(pool<Mutex>* pool) noexcept
{
    const pool_constants& P = pool->P;
    pool_state& s = pool->s;
    if (s.arena && P.is_deallocation_lock_free())
        call_with_num_bitmap_words(P.num_bitmap_words_per_slab, [&](auto num_words) {
            merge_deallocated_bits<decltype(num_words)::value>(P, s);
        });
    fm_assert(atomic_load(&s.num_allocated_buffers, memory_order::relaxed) == 0);
    virtual_memory header_memory = move(s.header_memory);
    destroy_at(pool);
}

void* allocate_large_buffer(usize bytes, usize alignment) noexcept
{
    char* const p = virtual_memory::map_pages(bytes, alignment);
    poison_memory_region(p + bytes, round_up(bytes, virtual_memory::page_size()) - bytes);
    return p;
}

void deallocate_large_buffer(void* p, usize bytes) noexcept
{
    // ASan on Windows doesn't intercept VirtualFree, so the pages would stay poisoned after the release.
    unpoison_memory_region(p, round_up(bytes, virtual_memory::page_size()));
    virtual_memory::unmap_pages(static_cast<char*>(p), bytes);
}

u32 find_or_assign_pool_per_type_index(u32& index) noexcept
{
    u32 expected = atomic_load(&index, memory_order::relaxed);
    if (expected != (u32)-1)
        return expected;
    const u32 next = atomic_fetch_add(&num_pool_per_type_indexes, 1u, memory_order::relaxed);
    fm_assert(next < max_pool_per_type_indexes);
    if (atomic_compare_exchange(&index, expected, next, memory_order::relaxed))
        return next;
    else
        return expected;
}

template u32 find_or_create_nonfull_slab<1>(const pool_constants& P, pool_state& s);
template u32 find_or_create_nonfull_slab<2>(const pool_constants& P, pool_state& s);
template u32 find_or_create_nonfull_slab<4>(const pool_constants& P, pool_state& s);
template u32 find_or_create_nonfull_slab<8>(const pool_constants& P, pool_state& s);
template u32 find_or_create_nonfull_slab<16>(const pool_constants& P, pool_state& s);
template u32 find_or_create_nonfull_slab<32>(const pool_constants& P, pool_state& s);
template u32 find_or_create_nonfull_slab<64>(const pool_constants& P, pool_state& s);
template void merge_deallocated_bits<1>(const pool_constants& P, pool_state& s);
template void merge_deallocated_bits<2>(const pool_constants& P, pool_state& s);
template void merge_deallocated_bits<4>(const pool_constants& P, pool_state& s);
template void merge_deallocated_bits<8>(const pool_constants& P, pool_state& s);
template void merge_deallocated_bits<16>(const pool_constants& P, pool_state& s);
template void merge_deallocated_bits<32>(const pool_constants& P, pool_state& s);
template void merge_deallocated_bits<64>(const pool_constants& P, pool_state& s);
template void maybe_give_back_empty_slab<1>(const pool_constants& P, pool_state& s, u32 slab);
template void maybe_give_back_empty_slab<2>(const pool_constants& P, pool_state& s, u32 slab);
template void maybe_give_back_empty_slab<4>(const pool_constants& P, pool_state& s, u32 slab);
template void maybe_give_back_empty_slab<8>(const pool_constants& P, pool_state& s, u32 slab);
template void maybe_give_back_empty_slab<16>(const pool_constants& P, pool_state& s, u32 slab);
template void maybe_give_back_empty_slab<32>(const pool_constants& P, pool_state& s, u32 slab);
template void maybe_give_back_empty_slab<64>(const pool_constants& P, pool_state& s, u32 slab);

template struct pool<Spinlock>;
template struct pool<null_mutex>;
template pool<Spinlock>* create_pool(const pool_constants& P, arena* arena);
template pool<null_mutex>* create_pool(const pool_constants& P, arena* arena);
template void destroy_pool(pool<Spinlock>* pool) noexcept;
template void destroy_pool(pool<null_mutex>* pool) noexcept;

} // namespace floormat::slab::detail
