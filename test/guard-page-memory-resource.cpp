#include "app.hpp"
#include "compat/guard-page-memory-resource.hpp"
#include "compat/assert.hpp"
#include "compat/stl/thread.hpp"
#include "compat/virtual-memory.hpp"
#include <array>
#include <latch>
#include <memory_resource>
#include <type_traits>
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <sys/mman.h>
#endif

namespace floormat {

namespace {

constexpr usize max_alignment = guard_page_memory_resource::max_alignment;
static_assert(!std::is_base_of_v<std::pmr::memory_resource, guard_page_memory_resource>);

usize round_up(usize x, usize alignment) { return (x + alignment - 1) & ~(alignment - 1); }

#if fm_ASAN
bool is_poisoned(const void* p) { return __asan_address_is_poisoned(p) != 0; }
#endif

bool is_unmapped(const void* p)
{
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info;
    fm_assert(VirtualQuery(p, &info, sizeof info) == sizeof info);
    return info.State == MEM_FREE;
#else
    const usize page = virtual_memory::page_size();
#ifdef __linux__
    unsigned char residency;
#else
    char residency;
#endif
    return mincore((void*)((uptr)p & ~(uptr)(page - 1)), page, &residency) != 0 && errno == ENOMEM;
#endif
}

void test_buffer_placement(guard_page_memory_resource& r, usize size, usize alignment)
{
    const usize page = virtual_memory::page_size();
    const usize page_bytes = alignment > page ? alignment : page;
    char* const p = static_cast<char*>(r.allocate(size, alignment));
    char* const end = p + size;
    [[maybe_unused]] char* const pages_start = p - (uptr)p % page_bytes;
    char* const pages_end = (char*)round_up((uptr)end, page_bytes);
    const usize tail = round_up(size, alignment) - size;
    fm_assert((uptr)p % alignment == 0 && (usize)(pages_end - end) == tail);
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info;
    fm_assert(VirtualQuery(p, &info, sizeof info) == sizeof info);
    fm_assert(info.State == MEM_COMMIT && (char*)info.BaseAddress + info.RegionSize == pages_end);
    fm_assert((uptr)info.AllocationBase % 65536 == 0 && (char*)info.AllocationBase <= pages_start);
#endif
#if fm_ASAN
    fm_assert(!__asan_region_is_poisoned(p, size));
    char* const granule_start = p - (uptr)p % 8;
    if (granule_start > pages_start)
        fm_assert(is_poisoned(pages_start) && is_poisoned(granule_start - 1));
    if (tail)
        fm_assert(is_poisoned(end) && is_poisoned(pages_end - 1));
#endif
    for (usize i = 0; i < size; i++)
        p[i] = (char)(i * 31 + alignment);
    for (usize i = 0; i < size; i++)
        fm_assert(p[i] == (char)(i * 31 + alignment));
    r.deallocate(p, size, alignment);
    fm_assert(is_unmapped(p) && is_unmapped(end - 1));
#if fm_ASAN
    fm_assert(!__asan_region_is_poisoned(pages_start, (usize)(pages_end - pages_start)));
#endif
}

void test_alignments_and_sizes()
{
    const usize page = virtual_memory::page_size();
    guard_page_memory_resource r;
    for (usize a = 1; a <= max_alignment; a *= 2)
    {
        const usize sizes[] = { a, 7 * a, 5 * a + 1, 2 * page + a / 2 + 3 };
        for (usize size : sizes)
            test_buffer_placement(r, size, a);
    }
    void* const p = r.allocate(0, 16);
    fm_assert(p && (uptr)p % 16 == 0 && r.num_allocated_buffers() == 1);
    r.deallocate(p, 0, 16);
    fm_assert(r.num_allocated_buffers() == 0);
}

void test_two_live_buffers()
{
    const usize page = virtual_memory::page_size();
    guard_page_memory_resource r;
    char* const a = static_cast<char*>(r.allocate(24, 8));
    char* const b = static_cast<char*>(r.allocate(24, 8));
    fm_assert((uptr)a / page != (uptr)b / page && r.num_allocated_buffers() == 2);
    for (u32 i = 0; i < 24; i++)
    {
        a[i] = (char)(i * 7 + 3);
        b[i] = (char)(i * 11 + 5);
    }
    r.deallocate(a, 24, 8);
    fm_assert(r.num_allocated_buffers() == 1 && is_unmapped(a));
    for (u32 i = 0; i < 24; i++)
        fm_assert(b[i] == (char)(i * 11 + 5));
#if fm_ASAN
    fm_assert(!__asan_region_is_poisoned(b, 24));
#endif
    r.deallocate(b, 24, 8);
    fm_assert(r.num_allocated_buffers() == 0);
}

void test_many_buffers()
{
    constexpr u32 count = 2000;
    guard_page_memory_resource r;
    char* p[count];
    auto size_of = [](u32 i) { return (usize)(1 + i * 37 % 5003); };
    auto alignment_of = [](u32 i) { return usize{1} << i % 14; };
    for (u32 round = 0; round < 2; round++)
    {
        for (u32 i = 0; i < count; i++)
        {
            p[i] = static_cast<char*>(r.allocate(size_of(i), alignment_of(i)));
            fm_assert((uptr)p[i] % alignment_of(i) == 0);
            p[i][0] = (char)(i + round);
            p[i][size_of(i) - 1] = (char)(i * 3 + round);
        }
        fm_assert(r.num_allocated_buffers() == count);
        for (u32 k = 0; k < count; k++)
        {
            const u32 i = k * 7919 % count;
            fm_assert(p[i][0] == (char)(i + round) && p[i][size_of(i) - 1] == (char)(i * 3 + round));
            r.deallocate(p[i], size_of(i), alignment_of(i));
        }
        fm_assert(r.num_allocated_buffers() == 0);
    }
}

void test_two_objects_in_turn()
{
    guard_page_memory_resource r1, r2;
    for (u32 i = 0; i < 100; i++)
    {
        void* const a = r1.allocate(64, 16);
        r1.deallocate(a, 64, 16);
        void* const b = r2.allocate(64, 16);
        r2.deallocate(b, 64, 16);
        fm_assert(a != b);
    }
    fm_assert(r1.num_allocated_buffers() == 0 && r2.num_allocated_buffers() == 0);
}

void test_allocating_threads()
{
    constexpr u32 num_threads = 4, count = 256;
    guard_page_memory_resource r;
    std::latch start_gun{num_threads};
    std::array<std::thread, num_threads> threads;
    for (u32 t = 0; t < num_threads; t++)
        threads[t] = std::thread{[&, t] {
            start_gun.arrive_and_wait();
            char* p[count];
            for (u32 i = 0; i < count; i++)
            {
                p[i] = static_cast<char*>(r.allocate(48, 16));
                p[i][47] = (char)(t * count + i);
            }
            for (u32 i = 0; i < count; i++)
            {
                fm_assert(p[i][47] == (char)(t * count + i));
                r.deallocate(p[i], 48, 16);
            }
        }};
    for (u32 t = 0; t < num_threads; t++)
        threads[t].join();
    fm_assert(r.num_allocated_buffers() == 0);
}

} // namespace

void Test::test_guard_page_memory_resource()
{
    test_alignments_and_sizes();
    test_two_live_buffers();
    test_many_buffers();
    test_two_objects_in_turn();
    test_allocating_threads();
}

} // namespace floormat
