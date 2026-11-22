#include "compat/slab-allocator.inl"
#include "compat/slab-allocator-std.hpp"
#include "compat/nanosecond.hpp"
#include "compat/timer.hpp"
#include <algorithm>
#include <new>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include <cr/Array.h>
#include <benchmark/benchmark.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#elif defined __linux__
#include <cstdio>
#endif

namespace floormat {

namespace {

struct allocated_object { u64 words[8]; };
static_assert(sizeof(allocated_object) == 64);

constexpr usize object_bytes = sizeof(allocated_object), object_align = alignof(allocated_object);

struct larger_object { u64 words[24]; };

constexpr usize larger_object_bytes = sizeof(larger_object), larger_object_align = alignof(larger_object);

struct new_delete_resource
{
    static constexpr usize max_alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    template<usize Size, usize Align, typename T = void> void* allocate();
    template<usize Size, usize Align, typename T = void> void deallocate(void* p) noexcept;
    template<usize Align> void* allocate_array(usize bytes);
    template<usize Align> void deallocate_array(void* p, usize bytes) noexcept;
};

template<usize Size, usize Align, typename T>
void* new_delete_resource::allocate()
{
    return ::operator new(Size);
}

template<usize Size, usize Align, typename T>
void new_delete_resource::deallocate(void* p) noexcept
{
#ifdef __cpp_sized_deallocation
    ::operator delete(p, Size);
#else
    ::operator delete(p);
#endif
}

template<usize Align>
void* new_delete_resource::allocate_array(usize bytes)
{
    return ::operator new(bytes);
}

template<usize Align>
void new_delete_resource::deallocate_array(void* p, usize bytes) noexcept
{
#ifdef __cpp_sized_deallocation
    ::operator delete(p, bytes);
#else
    (void)bytes;
    ::operator delete(p);
#endif
}

struct bench_tag;

constexpr slab::tunables without_guard_pages(slab::thread_model thread_model)
{
    return {.thread_model = thread_model, .buffer_source = slab::buffer_source::slabs};
}

constexpr slab::tunables without_guard_pages_or_superpages(slab::thread_model thread_model)
{
    return {.bytes_until_superpages = slab::tunables::no_superpages, .thread_model = thread_model,
            .buffer_source = slab::buffer_source::slabs};
}

// MSVC 19.53 rejects a designated initializer naming a template parameter in an alias template's argument.
template<slab::thread_model ThreadModel>
using slab_resource = slab::memory_resource<bench_tag, without_guard_pages(ThreadModel)>;
template<slab::thread_model ThreadModel>
using slab_resource_without_superpages =
    slab::memory_resource<bench_tag, without_guard_pages_or_superpages(ThreadModel)>;

constexpr slab::tunables with_arrays_mapped_above_largest_slab(slab::thread_model thread_model)
{
    slab::tunables tunables = without_guard_pages(thread_model);
    tunables.max_array_extent_bytes = tunables.max_slab_bytes;
    return tunables;
}

template<slab::thread_model ThreadModel>
using slab_resource_with_arrays_mapped_above_largest_slab =
    slab::memory_resource<bench_tag, with_arrays_mapped_above_largest_slab(ThreadModel)>;

constexpr slab::tunables without_pool_per_type(slab::thread_model thread_model)
{
    slab::tunables tunables = without_guard_pages(thread_model);
    tunables.has_pool_per_type = false;
    return tunables;
}

template<slab::thread_model ThreadModel>
using slab_resource_without_pool_per_type = slab::memory_resource<bench_tag, without_pool_per_type(ThreadModel)>;

constexpr slab::tunables with_2_mib_slabs = {.max_slab_bytes = 2 << 20, .buffer_source = slab::buffer_source::slabs};
using slab_resource_with_2_mib_slabs = slab::memory_resource<bench_tag, with_2_mib_slabs>;

constexpr slab::tunables with_superpages_from_arena_base(slab::thread_model thread_model)
{
    return {.bytes_until_superpages = 0, .thread_model = thread_model, .buffer_source = slab::buffer_source::slabs};
}

template<slab::thread_model ThreadModel>
using slab_resource_with_superpages_from_arena_base =
    slab::memory_resource<bench_tag, with_superpages_from_arena_base(ThreadModel)>;

#ifdef _WIN32
template<typename T>
double share_on_superpages(const Array<T*>& objects)
{
    Array<PSAPI_WORKING_SET_EX_INFORMATION> info{ValueInit, objects.size()};
    for (u32 i = 0; i < objects.size(); i++)
        info[i].VirtualAddress = objects[i];
    fm_assert(QueryWorkingSetEx(GetCurrentProcess(), info.data(), DWORD(sizeof(info[0]) * info.size())));
    u32 num_on_large_pages = 0;
    for (const PSAPI_WORKING_SET_EX_INFORMATION& x : info)
        num_on_large_pages += x.VirtualAttributes.Valid && x.VirtualAttributes.LargePage;
    return (double)num_on_large_pages / (double)objects.size();
}
#elif defined __linux__
template<typename T>
double share_on_superpages(const Array<T*>& objects)
{
    const auto [lowest, highest] = std::minmax_element(objects.begin(), objects.end());
    const auto first = (unsigned long)*lowest, last = (unsigned long)*highest;
    std::FILE* const f = std::fopen("/proc/self/smaps", "r");
    fm_assert(f);
    char line[512];
    bool is_in_range = false;
    unsigned long start = 0, end = 0, kib = 0, rss_kib = 0, huge_page_kib = 0;
    while (std::fgets(line, sizeof line, f))
    {
        if (std::sscanf(line, "%lx-%lx", &start, &end) == 2)
            is_in_range = start <= last && end > first;
        else if (is_in_range && std::sscanf(line, "Rss: %lu kB", &kib) == 1)
            rss_kib += kib;
        else if (is_in_range && std::sscanf(line, "AnonHugePages: %lu kB", &kib) == 1)
            huge_page_kib += kib;
    }
    std::fclose(f);
    return rss_kib ? (double)huge_page_kib / (double)rss_kib : 0;
}
#else
template<typename T>
double share_on_superpages(const Array<T*>&)
{
    return -1;
}
#endif

template<typename Resource>
allocated_object* allocate_object(Resource& r)
{
    auto* const p = ::new (r.template allocate<object_bytes, object_align>()) allocated_object;
    p->words[0] = 0;
    return p;
}

template<typename Resource>
void deallocate_object(Resource& r, allocated_object* p) noexcept
{
    r.template deallocate<object_bytes, object_align>(p);
}

template<typename Resource>
Array<allocated_object*> allocate_objects(Resource& r, u32 n)
{
    Array<allocated_object*> objects{NoInit, n};
    for (u32 i = 0; i < n; i++)
        objects[i] = allocate_object(r);
    return objects;
}

template<typename Resource>
void deallocate_objects(Resource& r, const Array<allocated_object*>& objects) noexcept
{
    for (allocated_object* p : objects)
        deallocate_object(r, p);
}

Array<u32> shuffled_indices(u32 n, std::mt19937_64& rng)
{
    Array<u32> indices{NoInit, n};
    for (u32 i = 0; i < n; i++)
        indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    return indices;
}

// Each case makes its own resource, since a merge's cost grows with the slabs its pool ever took.
template<typename Resource>
void Slab_Pairs(benchmark::State& state)
{
    Resource r;
    allocated_object* const live_object = allocate_object(r);
    for (auto _ : state)
    {
        allocated_object* p = allocate_object(r);
        benchmark::DoNotOptimize(p);
        deallocate_object(r, p);
    }
    deallocate_object(r, live_object);
    state.SetItemsProcessed(state.iterations());
}

template<typename Resource, u32 N>
void Slab_AllocateThenDeallocate(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects{NoInit, N};
    for (auto _ : state)
    {
        for (u32 i = 0; i < N; i++)
            objects[i] = allocate_object(r);
        benchmark::ClobberMemory();
        for (u32 i = 0; i < N; i++)
            deallocate_object(r, objects[i]);
    }
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_ShuffledDeallocate(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_objects(r, N);
    std::mt19937_64 rng{0x9e3779b97f4a7c15};
    const Array<u32> order = shuffled_indices(N, rng);
    for (auto _ : state)
    {
        Time start_time = Time::now();
        for (u32 i = 0; i < N; i++)
            deallocate_object(r, objects[order[i]]);
        benchmark::ClobberMemory();
        state.SetIterationTime((double)start_time.update().stamp * 1e-9);
        for (u32 i = 0; i < N; i++)
            objects[i] = allocate_object(r);
        benchmark::ClobberMemory();
    }
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_ShuffledDeallocateThenAllocate(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_objects(r, N);
    std::mt19937_64 rng{0x9e3779b97f4a7c15};
    const Array<u32> order = shuffled_indices(N, rng);
    for (auto _ : state)
    {
        for (u32 i = 0; i < N; i++)
            deallocate_object(r, objects[order[i]]);
        benchmark::ClobberMemory();
        for (u32 i = 0; i < N; i++)
            objects[i] = allocate_object(r);
        benchmark::ClobberMemory();
    }
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_RandomReplacement(benchmark::State& state)
{
    constexpr u32 num_replacements_per_iteration = N < 4096 ? N : 4096, num_iterations_before_repeat = 16;
    Resource r;
    Array<allocated_object*> objects = allocate_objects(r, N);
    std::mt19937_64 rng{0x9e3779b97f4a7c15};
    Array<u32> indices{NoInit, N};
    for (u32 i = 0; i < N; i++)
        indices[i] = i;
    Array<u32> replaced_indices{NoInit, num_iterations_before_repeat * num_replacements_per_iteration};
    for (u32 k = 0; k < num_iterations_before_repeat; k++)
        for (u32 i = 0; i < num_replacements_per_iteration; i++)
        {
            std::swap(indices[i], indices[std::uniform_int_distribution<u32>{i, N - 1}(rng)]);
            replaced_indices[k * num_replacements_per_iteration + i] = indices[i];
        }
    u32 first = 0;
    for (auto _ : state)
    {
        const u32* const batch = replaced_indices.data() + first;
        for (u32 i = 0; i < num_replacements_per_iteration; i++)
            deallocate_object(r, objects[batch[i]]);
        benchmark::ClobberMemory();
        for (u32 i = 0; i < num_replacements_per_iteration; i++)
            objects[batch[i]] = allocate_object(r);
        benchmark::ClobberMemory();
        first = (first + num_replacements_per_iteration) % (u32)replaced_indices.size();
    }
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * num_replacements_per_iteration);
}

template<typename Resource>
Array<allocated_object*> allocate_and_replace_half(Resource& r, u32 n)
{
    Array<allocated_object*> objects = allocate_objects(r, n);
    std::mt19937_64 rng{0x9e3779b97f4a7c15};
    Array<u32> order = shuffled_indices(n, rng);
    for (u32 i = 0; i < n / 2; i++)
        deallocate_object(r, objects[order[i]]);
    std::shuffle(order.begin(), order.begin() + n / 2, rng);
    for (u32 i = 0; i < n / 2; i++)
        objects[order[i]] = allocate_object(r);
    return objects;
}

template<typename Resource, u32 N>
void Slab_ReadAfterReplacingHalf(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_and_replace_half(r, N);
    for (u32 i = 0; i < N; i++)
        objects[i]->words[0] = i;
    for (auto _ : state)
    {
        u64 sum = 0;
        for (const allocated_object* p : objects)
            sum += p->words[0];
        benchmark::DoNotOptimize(sum);
    }
    state.counters["superpage_share"] = share_on_superpages(objects);
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_ReadWholeObjectsAfterReplacingHalf(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_and_replace_half(r, N);
    for (u32 i = 0; i < N; i++)
        for (u32 k = 0; k < 8; k++)
            objects[i]->words[k] = i + k;
    for (auto _ : state)
    {
        u64 sum = 0;
        for (const allocated_object* p : objects)
            for (u32 k = 0; k < 8; k++)
                sum += p->words[k];
        benchmark::DoNotOptimize(sum);
    }
    state.counters["superpage_share"] = share_on_superpages(objects);
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_ReadInAllocationOrder(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_objects(r, N);
    for (u32 i = 0; i < N; i++)
        objects[i]->words[0] = i;
    for (auto _ : state)
    {
        u64 sum = 0;
        for (const allocated_object* p : objects)
            sum += p->words[0];
        benchmark::DoNotOptimize(sum);
    }
    state.counters["superpage_share"] = share_on_superpages(objects);
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_ReadInRandomOrder(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_objects(r, N);
    for (u32 i = 0; i < N; i++)
        objects[i]->words[0] = i;
    std::mt19937_64 rng{0x9e3779b97f4a7c15};
    std::shuffle(objects.begin(), objects.end(), rng);
    for (auto _ : state)
    {
        u64 sum = 0;
        for (const allocated_object* p : objects)
            sum += p->words[0];
        benchmark::DoNotOptimize(sum);
    }
    state.counters["superpage_share"] = share_on_superpages(objects);
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_ReadWholeObjectsInRandomOrder(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_objects(r, N);
    for (u32 i = 0; i < N; i++)
        for (u32 k = 0; k < 8; k++)
            objects[i]->words[k] = i + k;
    std::mt19937_64 rng{0x9e3779b97f4a7c15};
    std::shuffle(objects.begin(), objects.end(), rng);
    for (auto _ : state)
    {
        u64 sum = 0;
        for (const allocated_object* p : objects)
            for (u32 k = 0; k < 8; k++)
                sum += p->words[k];
        benchmark::DoNotOptimize(sum);
    }
    state.counters["superpage_share"] = share_on_superpages(objects);
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * N);
}

// Allocations of 8 to 256 bytes in the world_load_generated and json_atlas_defs traces, by requested size.
// Smaller ones are left out, since each object holds a u64.
constexpr usize mixed_sizes[] = {8,  16,  17,  19,  23,  24,  31,  32,  48,  56,  64,  72,  80,  88,
                                 96, 104, 112, 120, 128, 144, 152, 168, 192, 208, 224, 240, 248};
constexpr double mixed_size_weights[] = {84, 569,  28, 2,  2,    20782, 17, 2676, 987, 1, 25,  14080, 5, 6905,
                                         58, 1024, 4,  27, 4097, 5,     1024, 2,  21,  1,  257, 24,   11};
static_assert(std::size(mixed_sizes) == std::size(mixed_size_weights));
constexpr u32 num_mixed_sizes = std::size(mixed_sizes);

template<usize Size> struct mixed_size_object { char bytes[Size]; };

template<typename Resource, usize... I>
u64* allocate_mixed_size(Resource& r, u32 size_index, std::index_sequence<I...>)
{
    void* p = nullptr;
    ((size_index == I ? (void)(p = r.template allocate<mixed_sizes[I], 1, mixed_size_object<mixed_sizes[I]>>())
                      : (void)0),
     ...);
    auto* const object = static_cast<u64*>(p);
    *object = 0;
    return object;
}

template<typename Resource, usize... I>
void deallocate_mixed_size(Resource& r, u64* p, u32 size_index, std::index_sequence<I...>) noexcept
{
    ((size_index == I ? r.template deallocate<mixed_sizes[I], 1, mixed_size_object<mixed_sizes[I]>>(p) : (void)0),
     ...);
}

template<typename Resource, u32 N>
void Slab_MixedSizes(benchmark::State& state)
{
    constexpr auto sizes = std::make_index_sequence<num_mixed_sizes>{};
    Resource r;
    std::mt19937_64 rng{0x9e3779b97f4a7c15};
    std::discrete_distribution<u32> size_distribution{std::begin(mixed_size_weights), std::end(mixed_size_weights)};
    Array<u64*> objects{NoInit, N};
    Array<u8> size_indices{NoInit, N};
    for (u32 i = 0; i < N; i++)
    {
        size_indices[i] = (u8)size_distribution(rng);
        objects[i] = allocate_mixed_size(r, size_indices[i], sizes);
    }
    Array<u32> order = shuffled_indices(N, rng);
    for (u32 i = 0; i < N / 2; i++)
        deallocate_mixed_size(r, objects[order[i]], size_indices[order[i]], sizes);
    std::shuffle(order.begin(), order.begin() + N / 2, rng);
    for (u32 i = 0; i < N / 2; i++)
    {
        size_indices[order[i]] = (u8)size_distribution(rng);
        objects[order[i]] = allocate_mixed_size(r, size_indices[order[i]], sizes);
    }
    for (u32 i = 0; i < N; i++)
        *objects[i] = i;
    Array<u64*> read_order{NoInit, N};
    for (u32 i = 0; i < N; i++)
        read_order[i] = objects[i];
    std::shuffle(read_order.begin(), read_order.end(), rng);
    for (auto _ : state)
    {
        u64 sum = 0;
        for (const u64* p : read_order)
            sum += *p;
        benchmark::DoNotOptimize(sum);
    }
    state.counters["superpage_share"] = share_on_superpages(read_order);
    for (u32 i = 0; i < N; i++)
        deallocate_mixed_size(r, objects[i], size_indices[i], sizes);
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_PairsWithLiveBuffers(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_objects(r, N + 1);
    for (auto _ : state)
    {
        allocated_object* p = allocate_object(r);
        benchmark::DoNotOptimize(p);
        deallocate_object(r, p);
    }
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations());
}

template<typename Resource, u32 N>
void Slab_OneFreeBufferPerWord(benchmark::State& state)
{
    Resource r;
    Array<allocated_object*> objects = allocate_objects(r, N);
    for (auto _ : state)
    {
        for (u32 i = 0; i < N; i += 64)
            deallocate_object(r, objects[i]);
        benchmark::DoNotOptimize(r.template num_allocated_buffers<object_bytes, object_align>());
        Time start_time = Time::now();
        for (u32 i = 0; i < N; i += 64)
            objects[i] = allocate_object(r);
        benchmark::ClobberMemory();
        state.SetIterationTime((double)start_time.update().stamp * 1e-9);
    }
    deallocate_objects(r, objects);
    state.SetItemsProcessed(state.iterations() * (N / 64));
}

template<typename Resource, u32 N>
void Slab_TwoPoolsFilledInTurn(benchmark::State& state)
{
    Resource r;
    Array<void*> buffers{NoInit, N};
    for (auto _ : state)
    {
        for (u32 i = 0; i < N; i++)
            buffers[i] = r.template allocate<object_bytes, object_align>();
        benchmark::ClobberMemory();
        for (u32 i = 0; i < N; i++)
            r.template deallocate<object_bytes, object_align>(buffers[i]);
        for (u32 i = 0; i < N; i++)
            buffers[i] = r.template allocate<larger_object_bytes, larger_object_align>();
        benchmark::ClobberMemory();
        for (u32 i = 0; i < N; i++)
            r.template deallocate<larger_object_bytes, larger_object_align>(buffers[i]);
    }
    state.SetItemsProcessed(state.iterations() * 2 * N);
}

template<typename Resource, u32 N>
void Slab_VectorPushBack(benchmark::State& state)
{
    using allocator = slab::std_allocator<u64, Resource>;
    Resource r;
    for (auto _ : state)
    {
        std::vector<u64, allocator> v{allocator{&r}};
        for (u32 i = 0; i < N; i++)
            v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * N);
}

template<typename Resource, u32 N>
void Slab_ShortLivedVector(benchmark::State& state)
{
    using allocator = slab::std_allocator<u64, Resource>;
    Resource r;
    for (auto _ : state)
    {
        std::vector<u64, allocator> v(N, allocator{&r});
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations());
}

template<typename Resource, u32 N>
void register_read_cases_with_n(const std::string& name)
{
    const std::string suffix = "/" + name + "/n:" + std::to_string(N);
    benchmark::RegisterBenchmark("Slab_ReadAfterReplacingHalf" + suffix, &Slab_ReadAfterReplacingHalf<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_ReadWholeObjectsAfterReplacingHalf" + suffix,
                                 &Slab_ReadWholeObjectsAfterReplacingHalf<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_ReadInAllocationOrder" + suffix, &Slab_ReadInAllocationOrder<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
}

template<typename Resource, u32 N>
void register_random_read_cases_with_n(const std::string& name)
{
    const std::string suffix = "/" + name + "/n:" + std::to_string(N);
    benchmark::RegisterBenchmark("Slab_ReadInRandomOrder" + suffix, &Slab_ReadInRandomOrder<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_ReadWholeObjectsInRandomOrder" + suffix,
                                 &Slab_ReadWholeObjectsInRandomOrder<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_MixedSizes" + suffix, &Slab_MixedSizes<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
}

template<typename Resource, u32 N>
void register_mixed_sizes_case_with_n(const std::string& name)
{
    benchmark::RegisterBenchmark("Slab_MixedSizes/" + name + "/n:" + std::to_string(N), &Slab_MixedSizes<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
}

template<typename Resource>
void register_random_read_cases(const std::string& name)
{
    register_random_read_cases_with_n<Resource, 4096>(name);
    register_random_read_cases_with_n<Resource, 262144>(name);
    register_random_read_cases_with_n<Resource, 4194304>(name);
}

template<typename Resource, u32 N>
void register_cases_with_n(const std::string& name)
{
    const std::string suffix = "/" + name + "/n:" + std::to_string(N);
    benchmark::RegisterBenchmark("Slab_AllocateThenDeallocate" + suffix, &Slab_AllocateThenDeallocate<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_ShuffledDeallocate" + suffix, &Slab_ShuffledDeallocate<Resource, N>)
        ->Unit(benchmark::kMicrosecond)->UseManualTime();
    register_read_cases_with_n<Resource, N>(name);
    benchmark::RegisterBenchmark("Slab_TwoPoolsFilledInTurn" + suffix, &Slab_TwoPoolsFilledInTurn<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
}

template<typename Resource>
void register_vector_push_back_cases(const std::string& name)
{
    benchmark::RegisterBenchmark("Slab_VectorPushBack/" + name + "/n:100000", &Slab_VectorPushBack<Resource, 100000>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_VectorPushBack/" + name + "/n:262144", &Slab_VectorPushBack<Resource, 262144>)
        ->Unit(benchmark::kMicrosecond);
}

template<typename Resource>
void register_short_lived_vector_cases(const std::string& name)
{
    benchmark::RegisterBenchmark("Slab_ShortLivedVector/" + name + "/n:8", &Slab_ShortLivedVector<Resource, 8>);
    benchmark::RegisterBenchmark("Slab_ShortLivedVector/" + name + "/n:64", &Slab_ShortLivedVector<Resource, 64>);
}

template<typename Resource>
void register_cases(const std::string& name)
{
    benchmark::RegisterBenchmark("Slab_Pairs/" + name, &Slab_Pairs<Resource>);
    register_vector_push_back_cases<Resource>(name);
    register_short_lived_vector_cases<Resource>(name);
    register_cases_with_n<Resource, 4096>(name);
    register_cases_with_n<Resource, 262144>(name);
}

template<typename Resource, u32 N>
void register_replacement_cases_with_n(const std::string& name)
{
    const std::string suffix = "/" + name + "/n:" + std::to_string(N);
    benchmark::RegisterBenchmark("Slab_ShuffledDeallocateThenAllocate" + suffix,
                                 &Slab_ShuffledDeallocateThenAllocate<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_RandomReplacement" + suffix, &Slab_RandomReplacement<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
}

template<typename Resource>
void register_replacement_cases(const std::string& name)
{
    register_replacement_cases_with_n<Resource, 2048>(name);
    register_replacement_cases_with_n<Resource, 4096>(name);
    register_replacement_cases_with_n<Resource, 65536>(name);
    register_replacement_cases_with_n<Resource, 262144>(name);
    register_replacement_cases_with_n<Resource, 4194304>(name);
}

template<typename Resource, u32 N>
void register_merge_cases_with_n(const std::string& name)
{
    const std::string suffix = "/" + name + "/n:" + std::to_string(N);
    benchmark::RegisterBenchmark("Slab_PairsWithLiveBuffers" + suffix, &Slab_PairsWithLiveBuffers<Resource, N>);
    benchmark::RegisterBenchmark("Slab_OneFreeBufferPerWord" + suffix, &Slab_OneFreeBufferPerWord<Resource, N>)
        ->Unit(benchmark::kMicrosecond)->UseManualTime();
}

template<typename Resource>
void register_merge_cases(const std::string& name)
{
    register_merge_cases_with_n<Resource, 4096>(name);
    register_merge_cases_with_n<Resource, 262144>(name);
    register_merge_cases_with_n<Resource, 4194304>(name);
}

#ifdef FM_SLAB_BENCH_LARGE_HEAP
template<typename Resource>
void register_large_heap_cases(const std::string& name)
{
    constexpr u32 N = 16777216;
    const std::string suffix = "/" + name + "/n:" + std::to_string(N);
    register_replacement_cases_with_n<Resource, N>(name);
    benchmark::RegisterBenchmark("Slab_ShuffledDeallocate" + suffix, &Slab_ShuffledDeallocate<Resource, N>)
        ->Unit(benchmark::kMicrosecond)->UseManualTime();
    benchmark::RegisterBenchmark("Slab_AllocateThenDeallocate" + suffix, &Slab_AllocateThenDeallocate<Resource, N>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_PairsWithLiveBuffers" + suffix, &Slab_PairsWithLiveBuffers<Resource, N>);
}
#endif

[[maybe_unused]] const int registered = [] {
    using enum slab::thread_model;
    register_cases<new_delete_resource>("new_delete");
    register_cases<slab_resource<single_threaded>>("single_threaded");
    register_cases<slab_resource<single_allocating_thread>>("single_allocating_thread");
    benchmark::RegisterBenchmark("Slab_Pairs/multi_threaded", &Slab_Pairs<slab_resource<multi_threaded>>);
    register_replacement_cases<new_delete_resource>("new_delete");
    register_replacement_cases<slab_resource<single_threaded>>("single_threaded");
    register_replacement_cases<slab_resource<single_allocating_thread>>("single_allocating_thread");
    register_merge_cases<slab_resource<single_threaded>>("single_threaded");
    register_merge_cases<slab_resource<single_allocating_thread>>("single_allocating_thread");
    register_cases_with_n<slab_resource_without_superpages<single_threaded>, 262144>(
        "single_threaded/superpages:off");
    register_cases_with_n<slab_resource_without_superpages<single_allocating_thread>, 262144>(
        "single_allocating_thread/superpages:off");
    register_merge_cases_with_n<slab_resource_without_superpages<single_threaded>, 262144>(
        "single_threaded/superpages:off");
    register_merge_cases_with_n<slab_resource_without_superpages<single_allocating_thread>, 262144>(
        "single_allocating_thread/superpages:off");
    register_read_cases_with_n<new_delete_resource, 2048>("new_delete");
    register_read_cases_with_n<new_delete_resource, 16384>("new_delete");
    register_read_cases_with_n<slab_resource_without_superpages<single_threaded>, 2048>(
        "single_threaded/superpages:off");
    register_read_cases_with_n<slab_resource_without_superpages<single_threaded>, 4096>(
        "single_threaded/superpages:off");
    register_read_cases_with_n<slab_resource_without_superpages<single_threaded>, 16384>(
        "single_threaded/superpages:off");
    register_read_cases_with_n<slab_resource_with_superpages_from_arena_base<single_threaded>, 2048>(
        "single_threaded/superpages:from_arena_base");
    register_read_cases_with_n<slab_resource_with_superpages_from_arena_base<single_threaded>, 4096>(
        "single_threaded/superpages:from_arena_base");
    register_read_cases_with_n<slab_resource_with_superpages_from_arena_base<single_threaded>, 16384>(
        "single_threaded/superpages:from_arena_base");
    register_read_cases_with_n<slab_resource_with_superpages_from_arena_base<single_threaded>, 262144>(
        "single_threaded/superpages:from_arena_base");
    benchmark::RegisterBenchmark("Slab_VectorPushBack/single_threaded_2_mib_slabs/n:100000",
                                 &Slab_VectorPushBack<slab_resource_with_2_mib_slabs, 100000>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_VectorPushBack/new_delete/n:8000",
                                 &Slab_VectorPushBack<new_delete_resource, 8000>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Slab_VectorPushBack/single_threaded/n:8000",
                                 &Slab_VectorPushBack<slab_resource<slab::thread_model::single_threaded>, 8000>)
        ->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark(
        "Slab_VectorPushBack/single_threaded/arrays_above_largest_slab:mapped/n:8000",
        &Slab_VectorPushBack<slab_resource_with_arrays_mapped_above_largest_slab<single_threaded>, 8000>)
        ->Unit(benchmark::kMicrosecond);
    register_vector_push_back_cases<slab_resource_with_arrays_mapped_above_largest_slab<single_threaded>>(
        "single_threaded/arrays_above_largest_slab:mapped");
    register_vector_push_back_cases<slab_resource_with_arrays_mapped_above_largest_slab<single_allocating_thread>>(
        "single_allocating_thread/arrays_above_largest_slab:mapped");
    register_random_read_cases<new_delete_resource>("new_delete");
    register_random_read_cases<slab_resource<single_threaded>>("single_threaded");
    register_random_read_cases<slab_resource_with_superpages_from_arena_base<single_threaded>>(
        "single_threaded/superpages:from_arena_base");
    register_random_read_cases<slab_resource_without_superpages<single_threaded>>("single_threaded/superpages:off");
    register_short_lived_vector_cases<slab_resource_without_pool_per_type<single_threaded>>(
        "single_threaded/pool_per_type:off");
    register_mixed_sizes_case_with_n<slab_resource_without_pool_per_type<single_threaded>, 4096>(
        "single_threaded/pool_per_type:off");
    register_mixed_sizes_case_with_n<slab_resource_without_pool_per_type<single_threaded>, 262144>(
        "single_threaded/pool_per_type:off");
    register_mixed_sizes_case_with_n<slab_resource_without_pool_per_type<single_threaded>, 4194304>(
        "single_threaded/pool_per_type:off");
#ifdef FM_SLAB_BENCH_LARGE_HEAP
    register_large_heap_cases<new_delete_resource>("new_delete");
    register_large_heap_cases<slab_resource<single_threaded>>("single_threaded");
    register_large_heap_cases<slab_resource<single_allocating_thread>>("single_allocating_thread");
    constexpr u32 large_heap_n = 16777216;
    register_random_read_cases_with_n<new_delete_resource, large_heap_n>("new_delete");
    register_random_read_cases_with_n<slab_resource<single_threaded>, large_heap_n>("single_threaded");
    register_random_read_cases_with_n<slab_resource_with_superpages_from_arena_base<single_threaded>, large_heap_n>(
        "single_threaded/superpages:from_arena_base");
    register_random_read_cases_with_n<slab_resource_without_superpages<single_threaded>, large_heap_n>(
        "single_threaded/superpages:off");
    register_mixed_sizes_case_with_n<slab_resource_without_pool_per_type<single_threaded>, large_heap_n>(
        "single_threaded/pool_per_type:off");
#endif
    return 0;
}();

} // namespace

} // namespace floormat
