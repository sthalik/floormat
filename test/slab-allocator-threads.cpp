#include "app.hpp"
#include "slab-allocator-common.hpp"
#include "compat/slab-allocator.inl"
#include "compat/slab-allocator-std.hpp"
#include "compat/chase-lev-deque.inl"
#include <bit>
#include <utility>
#include <cr/Array.h>

namespace floormat {

using namespace floormat::slab_test;

namespace {

using enum slab::thread_model;

struct tag_reuse_within_64_allocations;
struct tag_reuse_order;
struct tag_merge_over_summary_words;
struct tag_merge_over_shared_words;
struct tag_lock_free_poisoning;
struct tag_live_bytes_with_deallocating_threads;
struct tag_one_allocating_thread;
struct tag_multi_threaded;
struct tag_deallocating_while_allocating;
struct tag_guard_pages_with_deallocating_threads;
struct tag_slab_given_back_in_merge;
struct tag_array_extents_freed_by_other_thread;
struct tag_pools_per_type_created_by_threads;

template<u32 I> struct distinct_16_byte_object { u64 words[2]; };

using Deque = ChaseLevDeque<stamped_buffer>;

void test_reuse_within_64_allocations()
{
    using R = slab::memory_resource<tag_reuse_within_64_allocations, without_guard_pages(single_allocating_thread)>;
    R r;
    const slab::allocator<test_object<64>, R> a{&r};
    char* const base = allocate_bytes(a);
    a.deallocate(base);
    bool is_returned[64] = {};
    for (u32 i = 1; i < 200; i++)
    {
        char* const p = allocate_bytes(a);
        const usize offset = (usize)(p - base);
        fm_assert(offset < 64 * 64 && offset % 64 == 0);
        is_returned[offset / 64] = true;
        a.deallocate(p);
    }
    for (bool x : is_returned)
        fm_assert(x);
    fm_assert(a.num_allocated_buffers() == 0 && a.num_committed_slabs() == 1);
}

template<slab::thread_model Model>
void test_reuse_order()
{
    using R = slab::memory_resource<tag_reuse_order, without_guard_pages(Model)>;
    R r;
    const slab::allocator<test_object<64>, R> a{&r};
    char* const base = allocate_bytes(a);
    a.deallocate(base);
    // The slab's first free-bitmap word and deallocation word, as allocate_buffer() and the merge keep them.
    u64 free_word = ~u64{1}, deallocated_word = 1;
    for (u32 i = 1; i < 200; i++)
    {
        u32 expected = 0;
        if constexpr (Model != single_threaded)
        {
            expected = (u32)std::countr_zero(free_word);
            free_word &= free_word - 1;
            if (!free_word)
            {
                free_word = deallocated_word;
                deallocated_word = 0;
            }
            deallocated_word |= u64{1} << expected;
        }
        char* const p = allocate_bytes(a);
        fm_assert(p == base + expected * 64);
        a.deallocate(p);
    }
    fm_assert(a.num_allocated_buffers() == 0 && a.num_committed_slabs() == 1);
}

void test_merge_over_summary_words()
{
    using R = slab::memory_resource<tag_merge_over_summary_words, without_guard_pages(single_allocating_thread)>;
    R r;
    const slab::allocator<test_object<16>, R> a{&r};
    constexpr u32 per_slab = 4096, slab_bytes = 65536, n = 9 * per_slab;
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)(i / per_slab) * slab_bytes + (i % per_slab) * 16);
    }

    constexpr u32 num_slabs = n / per_slab, num_freed = n - num_slabs;
    Array<u32> order{NoInit, num_freed};
    for (u32 i = 0, k = 0; i < n; i++)
        if (i % per_slab != 0)
            order[k++] = i;
    shuffle(order, 0x5851f42d4c957f2d);
    for (u32 k = 0; k < num_freed / 2; k++)
        a.deallocate(p[order[k]]);
    fm_assert(a.num_allocated_buffers() == n - num_freed / 2);
    for (u32 k = num_freed / 2; k < num_freed; k++)
        a.deallocate(p[order[k]]);
    fm_assert(a.num_allocated_buffers() == num_slabs && a.num_committed_slabs() == num_slabs);

    for (u32 i = 0; i < n; i++)
        if (i % per_slab != 0)
            fm_assert(allocate_bytes(a) == p[i]);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0 && a.num_committed_slabs() == 1);
}

bool is_freed_by_merge_test(u32 i, u32 per_slab)
{
    const u32 slab = i / per_slab;
    if (slab == 3 || slab == 7 || slab == 8 || slab == 12)
        return i % per_slab != 0;
    else
        return (slab == 0 || slab == 5 || slab == 10 || slab == 16) && i % per_slab == 4;
}

void test_merge_over_shared_words()
{
    using T = u32_array<1793>;
    using R = slab::memory_resource<tag_merge_over_shared_words, without_guard_pages(single_allocating_thread)>;
    R r;
    const slab::allocator<T, R> a{&r};
    constexpr u32 buffer_bytes = 7184, per_slab = 9, slab_bytes = 64656, num_slabs = 17, n = num_slabs * per_slab;
    static_assert(sizeof(T) == 7172 && n == 153);
    static_assert(R::pool_constants<sizeof(T), alignof(T)>.buffer_bytes == buffer_bytes);
    static_assert(R::pool_constants<sizeof(T), alignof(T)>.slab_bytes == slab_bytes);
    char* p[n];
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)(i / per_slab) * slab_bytes + (i % per_slab) * buffer_bytes);
    }
    fm_assert(a.num_committed_slabs() == num_slabs);

    u32 freed[n], order[n];
    u32 num_freed = 0;
    for (u32 i = 0; i < n; i++)
        if (is_freed_by_merge_test(i, per_slab))
            freed[num_freed++] = i;
    fm_assert(num_freed == 36);
    for (u32 k = 0; k < num_freed; k++)
        order[k] = freed[k];
    shuffle({order, num_freed}, 0x14057b7ef767814f);
    for (u32 k = 0; k < num_freed; k++)
        a.deallocate(p[order[k]]);
#if fm_ASAN
    for (u32 slab : {0u, 3u, 5u, 10u, 16u})
        for (u32 i = slab * per_slab; i < (slab + 1) * per_slab; i++)
        {
            fm_assert(is_poisoned(p[i] + 16) == is_freed_by_merge_test(i, per_slab));
            fm_assert(is_poisoned(p[i] + sizeof(T)) && is_poisoned(p[i] + buffer_bytes - 1));
        }
#endif
    fm_assert(a.num_allocated_buffers() == 117);

    for (u32 k = 0; k < num_freed; k++)
    {
        char* const q = allocate_bytes(a);
        fm_assert(q == p[freed[k]]);
#if fm_ASAN
        fm_assert(!is_poisoned(q + 16) && !is_poisoned(q + sizeof(T) - 1) && is_poisoned(q + sizeof(T)));
#endif
    }
    fm_assert(a.num_committed_slabs() == num_slabs);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);
}

void test_lock_free_poisoning()
{
#if fm_ASAN
    using R = slab::memory_resource<tag_lock_free_poisoning, without_guard_pages(single_allocating_thread)>;
    R r;
    const slab::allocator<test_object<64>, R> a{&r};
    char* const p = allocate_bytes(a);
    char* const q = allocate_bytes(a);
    a.deallocate(p);
    fm_assert(is_poisoned(p) && is_poisoned(p + 63) && !is_poisoned(q));
    a.deallocate(q);
    fm_assert(a.num_allocated_buffers() == 0);
#endif
}

void test_live_bytes_with_deallocating_threads()
{
    using R = slab::memory_resource<tag_live_bytes_with_deallocating_threads, without_guard_pages(multi_threaded)>;
    R r;
    const slab::allocator<stamped_12_byte_buffer, R> a{&r};
    constexpr u32 num_threads = 4, num_rounds = 800, buffers_per_round = 24;
    run_threads<num_threads>([&a](u32 t) {
        stamped_12_byte_buffer* buffers[buffers_per_round];
        for (u32 k = 0; k < num_rounds; k++)
        {
            const u32 serial = (t * num_rounds + k) * buffers_per_round;
            for (u32 i = 0; i < buffers_per_round; i++)
            {
                buffers[i] = a.construct();
                write_stamp(*buffers[i], serial + i);
            }
            for (u32 i = 0; i < buffers_per_round; i++)
            {
                fm_assert(read_stamp(*buffers[i]) == serial + i);
                a.destroy(buffers[i]);
            }
        }
    });
    fm_assert(a.num_allocated_buffers() == 0);
}

void test_one_allocating_thread()
{
    using R = slab::memory_resource<tag_one_allocating_thread, without_guard_pages(single_allocating_thread)>;
    R r;
    const slab::allocator<stamped_buffer, R> a{&r};
    constexpr u32 count = 40000, num_deallocating_threads = 3, max_buffers_out = 256;

    Array<u8> seen{ValueInit, count};
    volatile u32 num_buffers_out = 0;
    volatile u8 is_allocating_done = 0;
    ChaseLevBufferPool pool;
    Deque q{pool};

    auto release = [&](stamped_buffer* b) {
        const u32 serial = read_stamp(*b);
        fm_assert(serial < count);
        fm_assert(atomic_fetch_add(&seen[serial], u8{1}) == 0);
        a.destroy(b);
        atomic_fetch_sub(&num_buffers_out, 1u);
    };

    run_threads<num_deallocating_threads + 1>([&](u32 i) {
        if (i == 0)
        {
            for (u32 serial = 0; serial < count; serial++)
            {
                while (atomic_load(&num_buffers_out) > max_buffers_out)
                    std::this_thread::yield();
                stamped_buffer* const b = a.construct();
                write_stamp(*b, serial);
                atomic_fetch_add(&num_buffers_out, 1u);
                q.push(b);
            }
            atomic_store(&is_allocating_done, u8{1});
            while (stamped_buffer* const b = q.take())
                release(b);
        }
        else
        {
            for (;;)
            {
                if (stamped_buffer* const b = q.steal())
                    release(b);
                else if (atomic_load(&is_allocating_done))
                    break;
                else
                    std::this_thread::yield();
            }
        }
    });

    for (u32 i = 0; i < count; i++)
        fm_assert(seen[i] == 1);
    fm_assert(a.num_allocated_buffers() == 0);
}

void test_multi_threaded()
{
    using R = slab::memory_resource<tag_multi_threaded, without_guard_pages(multi_threaded)>;
    R r;
    const slab::allocator<stamped_buffer, R> a{&r};
    constexpr u32 num_threads = 4, rounds = 2000, per_round = 16, max_buffers_out = 64;
    constexpr u32 per_thread = rounds * per_round, count = num_threads * per_thread;

    Array<u8> seen{ValueInit, count};
    volatile u32 num_buffers_out[num_threads] = {};
    volatile u8 is_done[num_threads] = {};
    ChaseLevBufferPool pool;
    Deque q0{pool}, q1{pool}, q2{pool}, q3{pool};
    Deque* const queues[num_threads] = {&q0, &q1, &q2, &q3};

    auto release = [&](stamped_buffer* b, u32 owner) {
        const u32 serial = read_stamp(*b);
        fm_assert(serial / per_thread == owner);
        fm_assert(atomic_fetch_add(&seen[serial], u8{1}) == 0);
        a.destroy(b);
    };

    run_threads<num_threads>([&](u32 t) {
        const u32 previous = (t + num_threads - 1) % num_threads;
        Deque& own_queue = *queues[t];
        auto release_from_previous = [&] {
            if (stamped_buffer* const b = queues[previous]->steal())
            {
                release(b, previous);
                atomic_fetch_sub(&num_buffers_out[previous], 1u);
                return true;
            }
            else
                return false;
        };

        for (u32 k = 0; k < rounds; k++)
        {
            stamped_buffer* buffers[per_round];
            for (u32 i = 0; i < per_round; i++)
            {
                buffers[i] = a.construct();
                write_stamp(*buffers[i], t * per_thread + k * per_round + i);
            }
            for (u32 i = 0; i < per_round / 2; i++)
                release(buffers[i], t);
            for (u32 i = per_round / 2; i < per_round; i++)
            {
                atomic_fetch_add(&num_buffers_out[t], 1u);
                own_queue.push(buffers[i]);
            }
            while (atomic_load(&num_buffers_out[t]) > max_buffers_out)
                if (!release_from_previous())
                    std::this_thread::yield();
        }
        atomic_store(&is_done[t], u8{1});
        while (release_from_previous() || !atomic_load(&is_done[previous]) ||
               atomic_load(&num_buffers_out[previous]))
            std::this_thread::yield();
        while (stamped_buffer* const b = own_queue.take())
        {
            release(b, t);
            atomic_fetch_sub(&num_buffers_out[t], 1u);
        }
    });

    for (u32 i = 0; i < count; i++)
        fm_assert(seen[i] == 1);
    fm_assert(a.num_allocated_buffers() == 0);
}

constexpr slab::tunables with_8_kib_slabs(slab::thread_model thread_model)
{
    slab::tunables tunables = without_guard_pages(thread_model);
    tunables.max_slab_bytes = 8 << 10;
    return tunables;
}

template<slab::thread_model Model>
void test_deallocating_while_allocating()
{
    using R = slab::memory_resource<tag_deallocating_while_allocating, with_8_kib_slabs(Model)>;
    R r;
    const slab::allocator<stamped_buffer, R> a{&r};
    constexpr u32 num_deallocating_threads = 3, per_thread = 4096, count = num_deallocating_threads * per_thread;

    Array<stamped_buffer*> old_buffers{NoInit, count}, new_buffers{NoInit, count};
    for (u32 i = 0; i < count; i++)
    {
        old_buffers[i] = a.construct();
        write_stamp(*old_buffers[i], i);
    }
    run_threads<num_deallocating_threads + 1>([&](u32 t) {
        if (t == 0)
        {
            for (u32 i = 0; i < count; i++)
            {
                new_buffers[i] = a.construct();
                write_stamp(*new_buffers[i], count + i);
            }
        }
        else
        {
            for (u32 i = (t - 1) * per_thread; i < t * per_thread; i++)
            {
                fm_assert(read_stamp(*old_buffers[i]) == i);
                a.destroy(old_buffers[i]);
            }
        }
    });
    for (u32 i = 0; i < count; i++)
    {
        fm_assert(read_stamp(*new_buffers[i]) == count + i);
        a.destroy(new_buffers[i]);
    }
    fm_assert(a.num_allocated_buffers() == 0);
}

void test_guard_pages_with_deallocating_threads()
{
    constexpr u32 num_threads = 4, count = 1024;
    constexpr slab::tunables tunables{.thread_model = single_allocating_thread,
                                     .buffer_source = slab::buffer_source::guard_pages};
    using R = slab::memory_resource<tag_guard_pages_with_deallocating_threads, tunables>;
    R r;
    char* p[count];
    for (u32 i = 0; i < count; i++)
        p[i] = static_cast<char*>(r.allocate<64, 8>());
    run_threads<num_threads>([&](u32 t) {
        for (u32 i = t; i < count; i += num_threads)
            r.deallocate<64, 8>(p[i]);
    });
    fm_assert(r.num_allocated_buffers<64, 8>() == 0);
}

template<slab::thread_model Model>
void test_slab_given_back_in_merge()
{
    using R = slab::memory_resource<tag_slab_given_back_in_merge, without_guard_pages(Model)>;
    R r;
    const slab::allocator<test_object<1024>, R> a{&r};
    const slab::allocator<test_object<48>, R> b{&r};
    constexpr u32 per_slab = 64, n = 3 * per_slab, slab_bytes = 65536;
    char* p[n];
    for (u32 i = 0; i < n; i++)
        p[i] = allocate_bytes(a);
    char* const base = r.arena().base;
    const u32 bump_offset = r.arena().bump_offset;
    run_threads<1>([&](u32) {
        for (u32 i = per_slab; i < n; i++)
            a.deallocate(p[i]);
    });
    fm_assert(a.num_committed_slabs() == 2);
    char* const q = allocate_bytes(b);
    fm_assert((q == base + slab_bytes || q == base + 2 * slab_bytes) && r.arena().bump_offset == bump_offset);
    b.deallocate(q);
    for (u32 i = 0; i < per_slab; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

template<slab::thread_model Model>
void test_array_extents_freed_by_other_thread()
{
    using R = slab::memory_resource<tag_array_extents_freed_by_other_thread, without_guard_pages(Model)>;
    R r;
    slab::std_allocator<char, R> a{&r};
    constexpr u32 n = 4096, bytes = 8192;
    Array<char*> p{NoInit, n}, q{NoInit, n};
    for (u32 i = 0; i < n; i++)
        p[i] = a.allocate(bytes);
    run_threads<2>([&](u32 t) {
        for (u32 i = 0; i < n; i++)
            if (t == 0)
                a.deallocate(p[i], bytes);
            else
            {
                q[i] = a.allocate(bytes);
                write_pattern(q[i], 64, i);
            }
    });
    for (u32 i = 0; i < n; i++)
    {
        fm_assert(has_pattern(q[i], 64, i));
        a.deallocate(q[i], bytes);
    }
    p[0] = a.allocate(2 * bytes);
    fm_assert(p[0] == r.arena().base);
    a.deallocate(p[0], 2 * bytes);
}

template<u32... I>
void test_pools_per_type_created_by_threads(std::integer_sequence<u32, I...>)
{
    using R = slab::memory_resource<tag_pools_per_type_created_by_threads, with_pool_per_type(multi_threaded)>;
    constexpr u32 num_threads = 4, num_types = sizeof...(I), num_objects_per_thread_and_type = 61;
    constexpr u32 num_objects_per_type = num_threads * num_objects_per_thread_and_type;
    R r;
    Array<void*> p{NoInit, num_types * num_objects_per_type};
    auto object_at = [&](u32 t, u32 i, u32 k) -> void*& {
        return p[(t * num_types + i) * num_objects_per_thread_and_type + k];
    };
    run_threads<num_threads>([&](u32 t) {
        for (u32 k = 0; k < num_objects_per_thread_and_type; k++)
            for (u32 j = 0; j < num_types; j++)
                ((I == (j + t) % num_types
                  ? (void)(object_at(t, I, k) = r.allocate<16, 8, distinct_16_byte_object<I>>())
                  : (void)0), ...);
    });
    fm_assert(((r.num_allocated_buffers<16, 8, distinct_16_byte_object<I>>() == num_objects_per_type) && ...));
    for (u32 t = 0; t < num_threads; t++)
        for (u32 k = 0; k < num_objects_per_thread_and_type; k++)
            (r.deallocate<16, 8, distinct_16_byte_object<I>>(object_at(t, I, k)), ...);
    fm_assert(((r.num_allocated_buffers<16, 8, distinct_16_byte_object<I>>() == 0) && ...));
}

} // namespace

void Test::test_slab_allocator_threads()
{
    test_reuse_within_64_allocations();
    test_reuse_order<single_threaded>();
    test_reuse_order<single_allocating_thread>();
    test_reuse_order<multi_threaded>();
    test_merge_over_summary_words();
    test_merge_over_shared_words();
    test_lock_free_poisoning();
    test_live_bytes_with_deallocating_threads();
    test_one_allocating_thread();
    test_multi_threaded();
    test_deallocating_while_allocating<single_allocating_thread>();
    test_deallocating_while_allocating<multi_threaded>();
    test_guard_pages_with_deallocating_threads();
    test_slab_given_back_in_merge<single_allocating_thread>();
    test_slab_given_back_in_merge<multi_threaded>();
    test_array_extents_freed_by_other_thread<single_allocating_thread>();
    test_array_extents_freed_by_other_thread<multi_threaded>();
    test_pools_per_type_created_by_threads(std::make_integer_sequence<u32, 8>{});
}

} // namespace floormat
