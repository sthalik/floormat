#pragma once
#include "compat/slab-allocator.hpp"
#include "compat/enumerate.hpp"
#include "compat/stl/thread.hpp"
#include <array>
#include <latch>

namespace floormat::slab_test {

template<usize Size> struct test_object { char bytes[Size]; };
template<usize N> struct u32_array { u32 values[N]; };
struct stamped_buffer { u64 words[8]; };
struct stamped_12_byte_buffer { u32 words[3]; };

#if fm_ASAN
bool is_poisoned(const void* p);
bool is_owned_by_asan_allocator(const void* p);
#endif
void write_stamp(stamped_buffer& b, u32 serial);
u32 read_stamp(const stamped_buffer& b);
void write_stamp(stamped_12_byte_buffer& b, u32 serial);
u32 read_stamp(const stamped_12_byte_buffer& b);
void write_pattern(char* p, u32 bytes, u32 serial);
bool has_pattern(const char* p, u32 bytes, u32 serial);
void shuffle(ArrayView<u32> values, u64 seed);
void test_slab_global_std_allocator();

constexpr slab::tunables without_guard_pages(slab::thread_model thread_model)
{
    return {.thread_model = thread_model, .buffer_source = slab::buffer_source::slabs,
            .is_empty_slab_given_back = true};
}

constexpr slab::tunables with_pool_per_type(slab::thread_model thread_model,
                                           slab::buffer_source buffer_source = slab::buffer_source::slabs)
{
    return {.thread_model = thread_model, .buffer_source = buffer_source, .has_pool_per_type = true,
            .is_empty_slab_given_back = true};
}

constexpr slab::tunables without_pool_per_type(slab::thread_model thread_model,
                                              slab::buffer_source buffer_source = slab::buffer_source::slabs)
{
    return {.thread_model = thread_model, .buffer_source = buffer_source, .has_pool_per_type = false,
            .is_empty_slab_given_back = true};
}

template<typename T, typename R>
char* allocate_bytes(const slab::allocator<T, R>& a)
{
    return static_cast<char*>(a.allocate());
}

template<u32 N, typename F>
void run_threads(F&& fn)
{
    std::latch start_gun(N);
    std::array<std::thread, N> threads;
    for (auto& [i, t] : enumerate(threads))
        t = std::thread([&, i] {
            start_gun.arrive_and_wait();
            fn(i);
        });
    for (u32 i = 0; i < N; ++i)
        threads[i].join();
}

} // namespace floormat::slab_test
