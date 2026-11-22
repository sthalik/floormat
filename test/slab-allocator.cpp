#include "app.hpp"
#include "compat/slab-allocator.inl"
#include "compat/slab-allocator-std.hpp"
#include "slab-allocator-common.hpp"
#include "compat/superpage.hpp"
#include "compat/virtual-memory.hpp"
#include <functional>
#include <initializer_list>
#include <list>
#include <memory>
#include <memory_resource>
#include <type_traits>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cr/Array.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#elif defined __linux__
#include <cstdio>
#include <cstring>
#endif

namespace floormat {

using namespace floormat::slab_test;

namespace {

using enum slab::thread_model;

constexpr slab::tunables without_guard_pages_or_superpages(slab::thread_model thread_model)
{
    return {.bytes_until_superpages = slab::tunables::no_superpages, .thread_model = thread_model,
            .buffer_source = slab::buffer_source::slabs};
}

template<typename R, usize Size, usize Align>
concept has_num_committed_slabs = requires (R& r) { r.template num_committed_slabs<Size, Align>(); };
template<typename R, usize Size, usize Align>
concept can_allocate = requires (R& r) { r.template allocate<Size, Align>(); };

static_assert(LockC<slab::detail::null_mutex>);

#ifdef _WIN32
DWORD memory_state(const void* p)
{
    MEMORY_BASIC_INFORMATION info;
    fm_assert(VirtualQuery(p, &info, sizeof info) == sizeof info);
    return info.State;
}
#endif

#if defined _WIN32 || defined __linux__
bool is_page_committed(const void* p)
{
#ifdef _WIN32
    return memory_state(p) == MEM_COMMIT;
#else
    // virtual_memory::commit() makes pages of a PROT_NONE reservation readable and writable.
    const auto address = (unsigned long)p;
    std::FILE* const f = std::fopen("/proc/self/maps", "r");
    fm_assert(f);
    char line[4096], perms[5] = {};
    unsigned long start = 0, end = 0;
    bool is_address_mapped = false;
    while (!is_address_mapped && std::fgets(line, sizeof line, f))
        is_address_mapped = std::sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3 &&
                            start <= address && address < end;
    std::fclose(f);
    fm_assert(is_address_mapped);
    return perms[0] == 'r' && perms[1] == 'w';
#endif
}
#endif

struct tag_size_classes;
struct tag_ascending_order;
struct tag_slab_boundary;
struct tag_freed_subset;
struct tag_pool_keeps_one_empty_slab;
struct tag_pool_keeps_empty_slabs_until_arena_grows;
struct tag_pool_shared_by_rounded_size;
struct tag_alignment_and_large_types;
struct tag_construct_and_destroy;
struct tag_poisoning;
struct tag_slabs_sharing_words;
struct tag_nonfull_slab_bitmap_growth;
struct tag_freed_subset_after_header_growth;
struct tag_slabs_of_two_pools;
struct tag_boundary_inside_unit;
struct tag_aligned_pools;
struct tag_merge_of_several_slabs;
struct tag_far_below_lowest_nonfull_slab;
struct tag_one_word_and_4096_buffers;
struct tag_large_buffers;
struct tag_resource_objects;
struct tag_arena_base_alignment;
struct tag_bytes_until_superpages;
struct tag_commits_past_bytes_until_superpages;
struct tag_arena_without_huge_pages_below_bytes_until_superpages;
struct tag_threads_creating_slabs_past_bytes_until_superpages;
struct tag_small_reservation_filled_to_last_slab;
struct tag_superpages_without_lock_memory_privilege;
struct tag_allocator_and_resource;
struct tag_std_allocator;
struct tag_std_containers;
struct tag_destroying_after_lock_free_deallocation;
struct tag_aligned_8_kib;
struct tag_guard_pages;
struct tag_operator_new;
struct tag_64_byte_alignment;
struct tag_slab_given_back_to_pool_of_other_size;
struct tag_freed_extent_merging_with_both_neighbours;
struct tag_slab_index_reused_with_other_extent;
struct tag_rest_below_4_kib_staying_inside_slab;
struct tag_4096_aligned_pool_taking_padding_from_free_extent;
struct tag_8192_aligned_pool_leaving_front_of_free_extent;
struct tag_free_extents_staying_in_their_resource;
struct tag_arrays_at_size_class_boundaries;
struct tag_arrays_of_two_sizes_sharing_pool;
struct tag_freed_array_extent_merging_and_serving_slab;
struct tag_vector_growth_keeping_elements;
struct tag_arena_counting_allocated_array_extents;
struct tag_array_extent_pending_until_next_extent;
struct tag_free_extent_of_more_than_4_mib;
struct tag_2_mib_array_at_8192_alignment;
struct tag_arrays_above_largest_slab;
struct tag_freed_1_mib_array_extent_reused;
struct tag_pools_of_types_with_one_rounded_size;
struct tag_pool_per_type_index_used_by_second_resource;
struct tag_std_allocator_objects_at_each_bitmap_word_count;
struct tag_arrays_of_each_size_class_through_std_allocator;
struct tag_pools_taking_back_their_given_back_slabs;
struct tag_64_byte_pool_slab_after_48_byte_pool_slab;
struct tag_given_back_slab_bitmap_committed_at_first_give_back;

// Returns true when the default tunables put num_buffers buffer_bytes-byte buffers in a slab_bytes-byte slab.
constexpr bool has_slab_size(u32 buffer_bytes, u32 num_buffers, u32 slab_bytes)
{
    const slab::detail::pool_constants P{buffer_bytes, 16, slab::tunables{}};
    return P.buffer_bytes == buffer_bytes && P.num_buffers_per_slab == num_buffers && P.slab_bytes == slab_bytes;
}

static_assert(has_slab_size(16, 4096, 65536));
static_assert(has_slab_size(48, 1365, 65520));
static_assert(has_slab_size(6000, 10, 60000));
static_assert(has_slab_size(7184, 9, 64656));
static_assert(has_slab_size(40960, 1, 40960));
static_assert(has_slab_size(65536, 1, 65536));
static_assert(slab::detail::pool_constants{7172, 4, slab::tunables{}}.buffer_bytes == 7184);
static_assert(slab::detail::pool_constants{48, 16, slab::tunables{}}.num_bitmap_words_per_slab == 32);
static_assert(slab::detail::pool_constants{6000, 16, slab::tunables{}}.num_bitmap_words_per_slab == 1);
static_assert(slab::detail::pool_constants{1, 1, slab::tunables{}}.buffer_bytes == 16);
static_assert(slab::detail::pool_constants{1, 1, slab::tunables{}}.max_slabs == (sizeof(void*) == 8 ? 1048575 : 1024));

using default_resource = slab::memory_resource<tag_size_classes>;
static_assert(default_resource::num_size_classes == 8184);
static_assert(default_resource::size_class_index(1, 1) == 0 && default_resource::size_class_index(65536, 16) == 4095);
static_assert(default_resource::size_class_index(32, 32) == 4096 && default_resource::size_class_index(65536, 8192) == 8183);
static_assert(default_resource::size_class_index(65537, 16) == (u32)-1);
static_assert(can_allocate<default_resource, 16, 8192> && !can_allocate<default_resource, 16, 16384>);
static_assert(!std::is_base_of_v<std::pmr::memory_resource, default_resource>);

// std::vector allows an incomplete element type, which slab::std_allocator must support.
struct self_referential_node
{
    std::vector<self_referential_node, slab::std_allocator<self_referential_node, default_resource>> children;
};

struct same_size_a { u64 x[6]; };
struct same_size_b { u64 y[6]; };
template<u32 I> struct distinct_16_byte_object { u64 words[2]; };
struct alignas(64) aligned_object { char bytes[80]; };
struct alignas(64) object_aligned_64 { char bytes[192]; };
struct alignas(4096) object_aligned_4096 { char bytes[4096]; };
struct alignas(8192) object_aligned_8192 { char bytes[3 * 8192]; };
struct alignas(8192) aligned_8_kib_object { char bytes[8192]; };
struct alignas(8192) large_aligned_object { char bytes[65536 + 8192]; };

struct product_or_sum
{
    u32 value;
    product_or_sum(u32 a, u32 b) noexcept;
    product_or_sum(std::initializer_list<u32> list) noexcept;
};

product_or_sum::product_or_sum(u32 a, u32 b) noexcept: value{a * b} {}

product_or_sum::product_or_sum(std::initializer_list<u32> list) noexcept: value{0}
{
    for (u32 x : list)
        value += x;
}

struct throwing_constructor
{
    u64 values[4];
    explicit throwing_constructor(u32 x);
};

throwing_constructor::throwing_constructor(u32 x): values{x, x, x, x}
{
    if (x == 13)
        throw x;
}

struct throwing_destructor
{
    u64 values[4];
    ~throwing_destructor() noexcept(false);
};

throwing_destructor::~throwing_destructor() noexcept(false)
{
    if (values[0] == 19)
        throw values[0];
}

void test_ascending_order()
{
    using R = slab::memory_resource<tag_ascending_order, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<24>, R> a{&r};
    constexpr u32 n = 37, buffer_bytes = 32;
    char* p[n];
    for (u32 i = 0; i < n; i++)
        p[i] = allocate_bytes(a);
    fm_assert(p[0] == r.arena().base);
    for (u32 i = 1; i < n; i++)
        fm_assert(p[i] == p[0] + i * buffer_bytes);
    a.deallocate(p[29]);
    a.deallocate(p[3]);
    a.deallocate(p[17]);
    fm_assert(a.num_allocated_buffers() == n - 3);
    fm_assert(allocate_bytes(a) == p[3]);
    fm_assert(allocate_bytes(a) == p[17]);
    fm_assert(allocate_bytes(a) == p[29]);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);
}

void test_slab_boundary()
{
    using R = slab::memory_resource<tag_slab_boundary, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<48>, R> a{&r};
    constexpr u32 per_slab = 1365, slab_bytes = 65520, n = 2 * per_slab;
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)(i / per_slab) * slab_bytes + (i % per_slab) * 48);
    }
    fm_assert((usize)(p[per_slab - 1] - p[0]) / 4096 == (usize)(p[per_slab] - p[0]) / 4096);
    a.deallocate(p[per_slab]);
    a.deallocate(p[per_slab - 1]);
    fm_assert(a.num_allocated_buffers() == n - 2);
    fm_assert(allocate_bytes(a) == p[per_slab - 1]);
    fm_assert(allocate_bytes(a) == p[per_slab]);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);
}

void test_freed_subset()
{
    using R = slab::memory_resource<tag_freed_subset, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<24>, R> a{&r};
    constexpr u32 buffer_bytes = 32, per_slab = 2048, slab_bytes = 65536, n = 5000;
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)(i / per_slab) * slab_bytes + (i % per_slab) * buffer_bytes);
    }

    Array<u32> freed{NoInit, n}, order{NoInit, n};
    u32 num_freed = 0;
    u64 x = 0x2545f4914f6cdd1d;
    for (u32 i = 0; i < n; i++)
    {
        x = x * 6364136223846793005 + 1442695040888963407;
        if ((x >> 62) == 0)
            freed[num_freed++] = i;
    }
    fm_assert(num_freed > 1000 && num_freed < 1500);
    for (u32 k = 0; k < num_freed; k++)
        order[k] = freed[k];
    shuffle(order.prefix(num_freed), x);
    for (u32 k = 0; k < num_freed; k++)
        a.deallocate(p[order[k]]);
    fm_assert(a.num_allocated_buffers() == n - num_freed);
    for (u32 k = 0; k < num_freed; k++)
        fm_assert(allocate_bytes(a) == p[freed[k]]);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);
}

void test_pool_keeps_one_empty_slab()
{
    using R = slab::memory_resource<tag_pool_keeps_one_empty_slab, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<1024>, R> a{&r};
    constexpr u32 per_slab = 64, n = 3 * per_slab, slab_bytes = 65536;
    char* p[n + 1];
    for (u32 i = 0; i < n; i++)
        p[i] = allocate_bytes(a);
    fm_assert(p[n - 1] == p[0] + 2 * slab_bytes + 63 * 1024);
    fm_assert(a.num_committed_slabs() == 3);
    for (u32 k = 0; k < 3; k++)
    {
        for (u32 i = k * per_slab; i < (k + 1) * per_slab; i++)
            a.deallocate(p[i]);
        fm_assert(a.num_committed_slabs() == (k == 0 ? 3 : 3 - k));
    }
    for (u32 i = 0; i <= per_slab; i++)
        p[i] = allocate_bytes(a);
    fm_assert(p[0] == r.arena().base && p[per_slab] == p[0] + slab_bytes);
    fm_assert(a.num_committed_slabs() == 2);
    for (u32 i = 0; i <= per_slab; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_committed_slabs() == 1);
    fm_assert(a.num_allocated_buffers() == 0);
}

void test_pool_keeps_empty_slabs_until_arena_grows()
{
    using R = slab::memory_resource<tag_pool_keeps_empty_slabs_until_arena_grows,
                                    slab::tunables{.buffer_source = slab::buffer_source::slabs}>;
    R r;
    const slab::allocator<test_object<1024>, R> a{&r};
    constexpr u32 n = 3 * 64;
    char* p[n];
    for (u32 i = 0; i < n; i++)
        p[i] = allocate_bytes(a);
    const u32 bump_offset = r.arena().bump_offset;
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_committed_slabs() == 3);
    for (u32 i = 0; i < n; i++)
        fm_assert(allocate_bytes(a) == p[i]);
    fm_assert(r.arena().bump_offset == bump_offset);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    void* const q = r.allocate_array<16>(3 << 16);
    fm_assert(a.num_committed_slabs() == 0 && r.arena().bump_offset == bump_offset);
    r.deallocate_array<16>(q, 3 << 16);
    fm_assert(allocate_bytes(a) == p[0] && r.arena().bump_offset == bump_offset);
    a.deallocate(p[0]);
    fm_assert(a.num_allocated_buffers() == 0);
}

#if defined _WIN32 || defined __linux__
void test_given_back_slab_bitmap_committed_at_first_give_back()
{
    using R = slab::memory_resource<tag_given_back_slab_bitmap_committed_at_first_give_back,
                                   without_guard_pages(single_threaded)>;
    constexpr u32 per_slab = 64, n = 2 * per_slab;
    constexpr u32 num_levels = R::pool_constants<1024, 1>.num_given_back_slab_bitmap_levels;
    R r;
    const slab::allocator<test_object<1024>, R> a{&r};
    char* p[n];
    for (char*& x : p)
        x = allocate_bytes(a);
    const slab::detail::pool_state& s = *r.pool_state<1024, 1, test_object<1024>>();
    fm_assert(a.num_committed_slabs() == 2);
    for (u32 level = 0; level < num_levels; level++)
        fm_assert(!is_page_committed(s.given_back_slab_bitmap[level]));
    for (char* x : p)
        a.deallocate(x);
    fm_assert(a.num_committed_slabs() == 1 && s.num_given_back_slabs == 1 && s.given_back_slab_bitmap[0][0] == 2);
    for (u32 level = 0; level < num_levels; level++)
        fm_assert(is_page_committed(s.given_back_slab_bitmap[level]));
}
#endif

void test_pool_shared_by_rounded_size()
{
    using R = slab::memory_resource<tag_pool_shared_by_rounded_size, without_pool_per_type(single_threaded)>;
    static_assert(R::size_class_index(40, 1) == R::size_class_index(48, 8) && R::size_class_index(49, 1) != R::size_class_index(48, 8));
    R r;
    const slab::allocator<test_object<40>, R> a{&r};
    const slab::allocator<same_size_a, R> b{&r};
    const slab::allocator<same_size_b, R> c{&r};
    const slab::allocator<test_object<49>, R> d{&r};
    char* const x = allocate_bytes(a);
    char* const y = allocate_bytes(b);
    char* const z = allocate_bytes(c);
    char* const w = allocate_bytes(d);
    fm_assert(y == x + 48 && z == x + 96);
    fm_assert(w == x + 65536);
    fm_assert(a.num_allocated_buffers() == 3 && c.num_allocated_buffers() == 3 && d.num_allocated_buffers() == 1);
    b.deallocate(x);
    a.deallocate(z);
    c.deallocate(y);
    d.deallocate(w);
    fm_assert(a.num_allocated_buffers() == 0 && d.num_allocated_buffers() == 0);
}

void test_alignment_and_large_types()
{
    using R = slab::memory_resource<tag_alignment_and_large_types, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<aligned_object, R> a{&r};
    static_assert(sizeof(aligned_object) == 128);
    char* p[3];
    for (u32 i = 0; i < 3; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert((uptr)p[i] % 64 == 0 && p[i] == p[0] + i * 128);
    }
    for (u32 i = 0; i < 3; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);

    const slab::allocator<test_object<8200>, R> b{&r};
    constexpr u32 per_slab = 7, slab_bytes = 57456;
    static_assert(R::pool_constants<8200, 1>.num_buffers_per_slab == per_slab);
    static_assert(R::pool_constants<8200, 1>.slab_bytes == slab_bytes);
    char* q[per_slab + 1];
    for (u32 i = 0; i <= per_slab; i++)
    {
        q[i] = allocate_bytes(b);
        q[i][0] = (char)(i + 61);
        q[i][8199] = (char)(i + 67);
    }
    fm_assert(q[1] == q[0] + 8208 && q[per_slab] == q[0] + slab_bytes);
    for (u32 i = 0; i <= per_slab; i++)
    {
        fm_assert(q[i][0] == (char)(i + 61) && q[i][8199] == (char)(i + 67));
        b.deallocate(q[i]);
    }
    fm_assert(b.num_allocated_buffers() == 0);

    const slab::allocator<test_object<65536>, R> c{&r};
    char* const s = allocate_bytes(c);
    char* const t = allocate_bytes(c);
    fm_assert(t == s + 65536);
    s[65535] = 71;
    t[0] = 73;
    fm_assert(s[65535] == 71 && t[0] == 73);
    c.deallocate(s);
    c.deallocate(t);
    fm_assert(c.num_allocated_buffers() == 0 && c.num_committed_slabs() == 1);
}

void test_construct_and_destroy()
{
    using R = slab::memory_resource<tag_construct_and_destroy, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<product_or_sum, R> a{&r};
    static_assert(noexcept(a.construct(3u, 5u)) && noexcept(a.construct_with_braces(3u, 5u)));
    product_or_sum* const x = a.construct(3u, 5u);
    product_or_sum* const y = a.construct_with_braces(3u, 5u);
    fm_assert(x->value == 15 && y->value == 8);
    fm_assert(a.num_allocated_buffers() == 2);
    a.destroy(x);
    a.destroy(y);
    fm_assert(a.num_allocated_buffers() == 0);

    const slab::allocator<throwing_constructor, R> b{&r};
    static_assert(!noexcept(b.construct(1u)) && !noexcept(b.construct_with_braces(1u)));
    void* const first = b.allocate();
    b.deallocate(first);
    u32 thrown = 0;
    try {
        (void)b.construct(13u);
    } catch (u32 value) {
        thrown = value;
    }
    fm_assert(thrown == 13 && b.num_allocated_buffers() == 0);
    thrown = 0;
    try {
        (void)b.construct_with_braces(13u);
    } catch (u32 value) {
        thrown = value;
    }
    fm_assert(thrown == 13 && b.num_allocated_buffers() == 0);
    throwing_constructor* const z = b.construct(7u);
    fm_assert(static_cast<void*>(z) == first && z->values[3] == 7);
    b.destroy(z);
    fm_assert(b.num_allocated_buffers() == 0);

    const slab::allocator<throwing_destructor, R> c{&r};
    static_assert(!noexcept(c.destroy(nullptr)));
    throwing_destructor* const w = c.construct();
    w->values[0] = 19;
    u64 thrown_by_destructor = 0;
    try {
        c.destroy(w);
    } catch (u64 value) {
        thrown_by_destructor = value;
    }
    fm_assert(thrown_by_destructor == 19 && c.num_allocated_buffers() == 0);
}

void test_poisoning()
{
#if fm_ASAN
    using R = slab::memory_resource<tag_poisoning, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<40>, R> a{&r};
    char* p[3];
    for (u32 i = 0; i < 3; i++)
        p[i] = allocate_bytes(a);
    fm_assert(!is_poisoned(p[0] + 39) && is_poisoned(p[0] + 40) && is_poisoned(p[0] + 47));
    a.deallocate(p[1]);
    fm_assert(is_poisoned(p[1]) && is_poisoned(p[1] + 39));
    fm_assert(!is_poisoned(p[0]) && !is_poisoned(p[0] + 39) && !is_poisoned(p[2]) && !is_poisoned(p[2] + 39));
    fm_assert(is_poisoned(p[2] + 48));
    char* const q = allocate_bytes(a);
    fm_assert(q == p[1] && !is_poisoned(q) && !is_poisoned(q + 39) && is_poisoned(q + 40));
    for (u32 i = 0; i < 3; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);
#endif
}

void test_slabs_sharing_words()
{
    using R = slab::memory_resource<tag_slabs_sharing_words, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<7168>, R> a{&r};
    constexpr u32 per_slab = 9, slab_bytes = 64512, num_slabs = 18, n = num_slabs * per_slab;
    static_assert(R::pool_constants<7168, 1>.num_buffers_per_slab == per_slab);
    static_assert(R::pool_constants<7168, 1>.slab_bytes == slab_bytes);
    char* p[n];
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)(i / per_slab) * slab_bytes + (i % per_slab) * 7168);
    }
    fm_assert(a.num_committed_slabs() == num_slabs);

    for (u32 slab = 12; slab >= 5; slab--)
        for (u32 k = per_slab; k-- > 1; )
            a.deallocate(p[slab * per_slab + k]);
    fm_assert(a.num_committed_slabs() == num_slabs);
    fm_assert(a.num_allocated_buffers() == n - 8 * (per_slab - 1));
    for (u32 slab = 5; slab <= 12; slab++)
        for (u32 k = 1; k < per_slab; k++)
            fm_assert(allocate_bytes(a) == p[slab * per_slab + k]);
    fm_assert(a.num_committed_slabs() == num_slabs);

    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0 && a.num_committed_slabs() == 1);
}

template<slab::thread_model Model>
void test_nonfull_slab_bitmap_growth()
{
    using R = slab::memory_resource<tag_nonfull_slab_bitmap_growth, without_guard_pages(Model)>;
    R r;
    const slab::allocator<test_object<32768>, R> a{&r};
    static_assert(R::template pool_constants<32768, 1>.num_buffers_per_slab == 2);
    // A page of the bitmap's lowest level covers 128 MiB of the arena, so these slabs span two of its pages.
    constexpr u32 num_slabs = 2100, n = 2 * num_slabs;
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)i * 32768);
    }
    fm_assert(a.num_committed_slabs() == num_slabs);

    Array<u32> freed{NoInit, num_slabs}, order{NoInit, num_slabs};
    u32 num_freed = 0;
    for (u32 i = 0; i < num_slabs; i++)
        if (i % 7 == 3 || i == num_slabs - 1)
            freed[num_freed++] = 2 * i + 1;
    for (u32 k = 0; k < num_freed; k++)
        order[k] = freed[k];
    shuffle(order.prefix(num_freed), 0x3c6ef372fe94f82b);
    for (u32 k = 0; k < num_freed; k++)
        a.deallocate(p[order[k]]);
    fm_assert(a.num_allocated_buffers() == n - num_freed);
    for (u32 k = 0; k < num_freed; k++)
        fm_assert(allocate_bytes(a) == p[freed[k]]);
    fm_assert(a.num_committed_slabs() == num_slabs);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);
}

template<slab::thread_model Model>
void test_freed_subset_after_header_growth(u32 n, u32 num_freed)
{
    using R = slab::memory_resource<tag_freed_subset_after_header_growth, without_guard_pages(Model)>;
    constexpr u32 per_slab = 4096, slab_bytes = 65536;
    R r;
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = static_cast<char*>(r.template allocate<1, 1>());
        fm_assert(p[i] == p[0] + (usize)(i / per_slab) * slab_bytes + (usize)(i % per_slab) * 16);
    }

    Array<u32> order{NoInit, n};
    for (u32 i = 0; i < n; i++)
        order[i] = i;
    shuffle(order, 0x2c1b3c6dd9e4f5a7);
    Array<bool> is_freed{ValueInit, n};
    for (u32 k = 0; k < num_freed; k++)
    {
        r.template deallocate<1, 1>(p[order[k]]);
        is_freed[order[k]] = true;
    }
    fm_assert((r.template num_allocated_buffers<1, 1>() == n - num_freed));
    for (u32 i = 0; i < n; i++)
        if (is_freed[i])
            fm_assert(r.template allocate<1, 1>() == p[i]);
    for (u32 i = 0; i < n; i++)
        r.template deallocate<1, 1>(p[i]);
    fm_assert((r.template num_allocated_buffers<1, 1>() == 0));
    fm_assert((r.template num_committed_slabs<1, 1>() == 1));
}

void test_slabs_of_two_pools_end_to_end()
{
    using R = slab::memory_resource<tag_slabs_of_two_pools, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<6000>, R> a{&r};
    const slab::allocator<test_object<7184>, R> b{&r};
    constexpr u32 a_bytes = 6000, a_per_slab = 10, b_bytes = 7184, b_per_slab = 9;
    constexpr u32 slab_offsets[4] = {0, 60000, 124656, 184656};
    constexpr u32 n = 2 * (a_per_slab + b_per_slab);
    char* p[n];
    bool is_from_b[n];
    u32 num_allocated = 0;
    for (u32 k = 0; k < 4; k++)
    {
        const bool is_b = k % 2 == 1;
        const u32 per_slab = is_b ? b_per_slab : a_per_slab, bytes = is_b ? b_bytes : a_bytes;
        for (u32 i = 0; i < per_slab; i++)
        {
            char* const q = is_b ? allocate_bytes(b) : allocate_bytes(a);
            fm_assert(q == r.arena().base + slab_offsets[k] + i * bytes);
            write_pattern(q, bytes, num_allocated);
            is_from_b[num_allocated] = is_b;
            p[num_allocated++] = q;
        }
    }
    auto bytes_of = [&](u32 j) { return is_from_b[j] ? b_bytes : a_bytes; };
    auto deallocate_at = [&](u32 j) {
        if (is_from_b[j])
            b.deallocate(p[j]);
        else
            a.deallocate(p[j]);
    };
    for (u32 j = 0; j < n; j++)
        fm_assert(has_pattern(p[j], bytes_of(j), j));

    // Each pair holds the last buffer of a slab and the first of the next one.
    constexpr u32 boundaries[3][2] = {{9, 10}, {18, 19}, {28, 29}};
    deallocate_at(boundaries[0][0]);
    deallocate_at(boundaries[1][1]);
    deallocate_at(boundaries[2][1]);
    for (u32 j : {boundaries[0][1], boundaries[1][0], boundaries[2][0]})
    {
        fm_assert(has_pattern(p[j], bytes_of(j), j));
#if fm_ASAN
        fm_assert(!is_poisoned(p[j]) && !is_poisoned(p[j] + bytes_of(j) - 1));
#endif
    }
    fm_assert(a.num_allocated_buffers() == 2 * a_per_slab - 2 && b.num_allocated_buffers() == 2 * b_per_slab - 1);
    fm_assert(allocate_bytes(a) == p[boundaries[0][0]]);
    fm_assert(allocate_bytes(a) == p[boundaries[1][1]]);
    fm_assert(allocate_bytes(b) == p[boundaries[2][1]]);
    for (u32 j = 0; j < n; j++)
        deallocate_at(j);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

template<slab::thread_model Model>
void test_deallocation_at_boundary_inside_unit()
{
    using R = slab::memory_resource<tag_boundary_inside_unit, without_guard_pages(Model)>;
    R r;
    const slab::allocator<test_object<48>, R> a{&r};
    const slab::allocator<test_object<16>, R> b{&r};
    constexpr u32 a_per_slab = 1365, b_per_slab = 4096;
    Array<char*> p{NoInit, a_per_slab + 1}, q{NoInit, b_per_slab};
    for (u32 i = 0; i < a_per_slab; i++)
        p[i] = allocate_bytes(a);
    for (u32 i = 0; i < b_per_slab; i++)
        q[i] = allocate_bytes(b);
    p[a_per_slab] = allocate_bytes(a);

    // b's slab begins in 4 KiB unit 15, where a's slab 0 ends. a's slab 1 begins in unit 31, where b's slab ends.
    char* const base = r.arena().base;
    fm_assert(p[a_per_slab - 1] == base + 15 * 4096 + 4032 && q[0] == base + 15 * 4096 + 4080);
    fm_assert(q[b_per_slab - 1] == base + 31 * 4096 + 4064 && p[a_per_slab] == base + 31 * 4096 + 4080);
    a.deallocate(p[a_per_slab - 1]);
    b.deallocate(q[0]);
    b.deallocate(q[b_per_slab - 1]);
    a.deallocate(p[a_per_slab]);
    fm_assert(a.num_allocated_buffers() == a_per_slab - 1 && b.num_allocated_buffers() == b_per_slab - 2);
    fm_assert(allocate_bytes(a) == p[a_per_slab - 1]);
    fm_assert(allocate_bytes(a) == p[a_per_slab]);
    fm_assert(allocate_bytes(b) == q[0]);
    fm_assert(allocate_bytes(b) == q[b_per_slab - 1]);
    for (char* x : p)
        a.deallocate(x);
    for (char* x : q)
        b.deallocate(x);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

// Allocates p.size() buffers from a's next slab, checking that the first lies at first_offset from base.
template<typename T, typename R>
void allocate_aligned_slab(const slab::allocator<T, R>& a, char* base, u32 first_offset, ArrayView<char*> p)
{
    for (u32 i = 0; i < p.size(); i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == base + first_offset + i * sizeof(T) && (uptr)p[i] % alignof(T) == 0);
    }
}

void test_aligned_pools_after_16_aligned_slab()
{
    using R = slab::memory_resource<tag_aligned_pools, without_guard_pages(single_threaded)>;
    static_assert(R::pool_constants<48, 1>.slab_bytes == 65520 && R::pool_constants<16, 1>.slab_bytes == 65536);
    static_assert(R::pool_constants<192, 64>.num_buffers_per_slab == 341);
    static_assert(R::pool_constants<192, 64>.slab_bytes == 65472);
    static_assert(R::pool_constants<4096, 4096>.num_buffers_per_slab == 16);
    static_assert(R::pool_constants<24576, 8192>.num_buffers_per_slab == 2);
    R r;
    const slab::allocator<test_object<48>, R> s48{&r};
    const slab::allocator<test_object<16>, R> s16{&r};
    const slab::allocator<test_object<6000>, R> s6000{&r};
    const slab::allocator<test_object<7184>, R> s7184{&r};
    const slab::allocator<object_aligned_64, R> a64{&r};
    const slab::allocator<object_aligned_4096, R> a4096{&r};
    const slab::allocator<object_aligned_8192, R> a8192{&r};
    char* const p48 = allocate_bytes(s48);
    char* const base = r.arena().base;
    fm_assert(p48 == base);
    char* p64[341];
    allocate_aligned_slab(a64, base, 65536, p64);
    char* const p16 = allocate_bytes(s16);
    fm_assert(p16 == base + 65536 + 65472);
    char* p4096[16];
    allocate_aligned_slab(a4096, base, 48 * 4096, p4096);
    char* const p6000 = allocate_bytes(s6000);
    fm_assert(p6000 == base + 64 * 4096);
    char* p8192[2];
    allocate_aligned_slab(a8192, base, 40 * 8192, p8192);
    char* const p7184 = allocate_bytes(s7184);
    fm_assert(p7184 == base + 40 * 8192 + 49152);
#if fm_ASAN
    fm_assert(is_poisoned(p48 + 65520) && is_poisoned(p64[0] - 1));
    fm_assert(is_poisoned(p16 + 65536) && is_poisoned(p4096[0] - 1));
#endif

    s48.deallocate(p48);
    s16.deallocate(p16);
    s6000.deallocate(p6000);
    s7184.deallocate(p7184);
    for (char* p : p64)
        a64.deallocate(p);
    for (char* p : p4096)
        a4096.deallocate(p);
    for (char* p : p8192)
        a8192.deallocate(p);
    fm_assert(s48.num_allocated_buffers() == 0 && a64.num_allocated_buffers() == 0);
    fm_assert(a4096.num_allocated_buffers() == 0 && a8192.num_allocated_buffers() == 0);
}

template<slab::thread_model Model>
void test_merge_of_several_slabs()
{
    using R = slab::memory_resource<tag_merge_of_several_slabs, without_guard_pages(Model)>;
    R r;
    const slab::allocator<test_object<1024>, R> a{&r};
    constexpr u32 per_slab = 64, n = 8 * per_slab;
    char* p[n];
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + i * 1024);
    }
    constexpr u32 freed[] = {6 * per_slab + 5, 2 * per_slab + 9, 7 * per_slab + 63, 5, 2 * per_slab + 40,
                             4 * per_slab};
    constexpr u32 freed_in_address_order[] = {5, 2 * per_slab + 9, 2 * per_slab + 40, 4 * per_slab,
                                              6 * per_slab + 5, 7 * per_slab + 63};

    // No slab has a free buffer, so the first allocation merges.
    for (u32 i : freed)
        a.deallocate(p[i]);
    for (u32 i : freed_in_address_order)
        fm_assert(allocate_bytes(a) == p[i]);
    char* const q = allocate_bytes(a);
    fm_assert(q == p[0] + n * 1024);
    a.deallocate(q);

    for (u32 k = std::size(freed); k-- > 0; )
        a.deallocate(p[freed[k]]);
    fm_assert(a.num_allocated_buffers() == n - std::size(freed));
    for (u32 i : freed_in_address_order)
        fm_assert(allocate_bytes(a) == p[i]);
    fm_assert(allocate_bytes(a) == q);
    a.deallocate(q);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);
}

template<slab::thread_model Model>
void test_deallocation_far_below_lowest_nonfull_slab()
{
    using R = slab::memory_resource<tag_far_below_lowest_nonfull_slab, without_guard_pages(Model)>;
    R r;
    const slab::allocator<test_object<1024>, R> a{&r};
    // Slab 3's bit and slab 300's lie in different words of the nonfull slab bitmap's two lowest levels.
    constexpr u32 per_slab = 64, n = 300 * per_slab + per_slab - 1, far_below = 3 * per_slab + 17;
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)i * 1024);
    }
    char* const last_buffer = p[0] + (usize)n * 1024;

    a.deallocate(p[far_below]);
    char* const x = allocate_bytes(a);
    char* const y = allocate_bytes(a);
    if constexpr (Model == single_threaded)
        fm_assert(x == p[far_below] && y == last_buffer);
    else
        fm_assert(x == last_buffer && y == p[far_below]);

    a.deallocate(last_buffer);
    fm_assert(a.num_allocated_buffers() == n);
    a.deallocate(p[per_slab + 9]);
    fm_assert(a.num_allocated_buffers() == n - 1);
    fm_assert(allocate_bytes(a) == p[per_slab + 9]);
    fm_assert(allocate_bytes(a) == last_buffer);
    a.deallocate(last_buffer);
    for (u32 i = 0; i < n; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0);
}

template<slab::thread_model Model>
void test_slabs_of_one_word_and_of_4096_buffers()
{
    using R = slab::memory_resource<tag_one_word_and_4096_buffers, without_guard_pages(Model)>;
    static_assert(R::template pool_constants<6000, 1>.num_bitmap_words_per_slab == 1);
    static_assert(R::template pool_constants<16, 1>.num_buffers_per_slab == 4096);
    R r;
    const slab::allocator<test_object<6000>, R> a{&r};
    const slab::allocator<test_object<16>, R> b{&r};
    constexpr u32 a_per_slab = 10, a_slab_bytes = 60000, num_a = 25, b_per_slab = 4096, num_b = 2 * b_per_slab + 3;
    char* p[num_a];
    Array<char*> q{NoInit, num_b};
    for (u32 i = 0; i < num_a; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (i / a_per_slab) * a_slab_bytes + (i % a_per_slab) * 6000);
    }
    for (u32 i = 0; i < num_b; i++)
    {
        q[i] = allocate_bytes(b);
        fm_assert(q[i] == q[0] + (usize)i * 16);
    }
    fm_assert(q[0] == p[0] + 3 * a_slab_bytes);

    for (u32 i : {24u, 0u, 13u, 9u})
        a.deallocate(p[i]);
    for (u32 i : {8194u, 4095u, 0u, 63u, 4096u, 64u, 2000u})
        b.deallocate(q[i]);
    fm_assert(a.num_allocated_buffers() == num_a - 4 && b.num_allocated_buffers() == num_b - 7);
    for (u32 i : {0u, 9u, 13u, 24u})
        fm_assert(allocate_bytes(a) == p[i]);
    for (u32 i : {0u, 63u, 64u, 2000u, 4095u, 4096u, 8194u})
        fm_assert(allocate_bytes(b) == q[i]);
    char* const x = allocate_bytes(a);
    char* const y = allocate_bytes(b);
    fm_assert(x == p[0] + 2 * a_slab_bytes + 5 * 6000 && y == q[0] + (usize)num_b * 16);
    a.deallocate(x);
    b.deallocate(y);
    for (char* z : p)
        a.deallocate(z);
    for (char* z : q)
        b.deallocate(z);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

constexpr slab::tunables with_arrays_mapped_above_largest_slab = [] {
    slab::tunables tunables = without_guard_pages(single_threaded);
    tunables.max_array_extent_bytes = tunables.max_slab_bytes;
    return tunables;
}();

void test_large_buffers()
{
    using R = slab::memory_resource<tag_large_buffers, with_arrays_mapped_above_largest_slab>;
    static_assert(R::size_class_index(65536 + 16, 1) == (u32)-1);
    static_assert(R::size_class_index(sizeof(large_aligned_object), 8192) == (u32)-1);
    R r;
    const usize page = virtual_memory::page_size();
    auto check_large_buffer = [page](char* p, usize bytes, usize alignment) {
        fm_assert((uptr)p % alignment == 0 && (uptr)p % page == 0);
        p[0] = 73;
        p[bytes - 1] = 79;
        fm_assert(p[0] == 73 && p[bytes - 1] == 79);
#if fm_ASAN
        fm_assert(!is_poisoned(p) && !is_poisoned(p + bytes - 1));
        fm_assert(bytes % page == 0 || is_poisoned(p + bytes));
#endif
#ifdef _WIN32
        fm_assert(memory_state(p) == MEM_COMMIT && memory_state(p + bytes - 1) == MEM_COMMIT);
#endif
    };
    auto check_unmapped = []([[maybe_unused]] const char* p) {
#ifdef _WIN32
        fm_assert(memory_state(p) == MEM_FREE);
#endif
    };

    const slab::allocator<test_object<65536 + 16>, R> a{&r};
    char* const p = allocate_bytes(a);
    check_large_buffer(p, 65536 + 16, 1);
    a.deallocate(p);
    check_unmapped(p);

    const slab::allocator<large_aligned_object, R> b{&r};
    char* const q = allocate_bytes(b);
    check_large_buffer(q, sizeof(large_aligned_object), 8192);
    b.deallocate(q);
    check_unmapped(q);

    slab::std_allocator<u64, R> c{&r};
    u64* const x = c.allocate(8200);
    check_large_buffer(reinterpret_cast<char*>(x), 8200 * sizeof(u64), alignof(u64));
    c.deallocate(x, 8200);
    check_unmapped(reinterpret_cast<char*>(x));

    slab::std_allocator<aligned_8_kib_object, R> d{&r};
    aligned_8_kib_object* const y = d.allocate(9);
    check_large_buffer(y->bytes, 9 * sizeof(aligned_8_kib_object), 8192);
    d.deallocate(y, 9);
    check_unmapped(reinterpret_cast<char*>(y));
    fm_assert(!r.arena().base);
}

void test_resource_objects()
{
    using R = slab::memory_resource<tag_resource_objects, without_guard_pages(single_threaded)>;
    auto is_inside = [](const void* p, const virtual_memory& m) { return (uptr)p - (uptr)m.data() < m.size(); };
    R r1;
    char* p[3];
    char* q;
    const char* pagemap;
    {
        R r2;
        for (u32 i = 0; i < 3; i++)
            p[i] = static_cast<char*>(r1.allocate<64, 8>());
        q = static_cast<char*>(r2.allocate<64, 8>());
        fm_assert(p[0] == r1.arena().base && p[1] == p[0] + 64 && p[2] == p[0] + 128 && q == r2.arena().base);
        const slab::detail::arena &a1 = r1.arena(), &a2 = r2.arena();
        fm_assert(!is_inside(a1.memory.data(), a2.memory) && !is_inside(a2.memory.data(), a1.memory));
        fm_assert(!is_inside(a1.pagemap_memory.data(), a2.pagemap_memory));
        fm_assert(!is_inside(a2.pagemap_memory.data(), a1.pagemap_memory));
        fm_assert((r1.num_allocated_buffers<64, 8>() == 3 && r2.num_allocated_buffers<64, 8>() == 1));
        r2.deallocate<64, 8>(q);
        fm_assert((r1.num_allocated_buffers<64, 8>() == 3 && r2.num_allocated_buffers<64, 8>() == 0));
#if fm_ASAN
        fm_assert(is_poisoned(q));
#endif
        pagemap = a2.pagemap_memory.data();
    }
#ifdef _WIN32
    fm_assert(memory_state(q) == MEM_FREE && memory_state(pagemap) == MEM_FREE);
    fm_assert(memory_state(p[0]) == MEM_COMMIT);
#endif
#if fm_ASAN
    fm_assert(!is_poisoned(q));
#endif
    for (u32 i = 0; i < 3; i++)
        r1.deallocate<64, 8>(p[i]);
    fm_assert((r1.num_allocated_buffers<64, 8>() == 0));
    (void)pagemap;
}

template<slab::tunables Tunables>
void test_arena_base_alignment()
{
    using R = slab::memory_resource<tag_arena_base_alignment, Tunables>;
    R r;
    const slab::allocator<test_object<16>, R> a{&r};
    const u32 n = 2 * (Tunables.max_slab_bytes / 16) + 1;
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)i * 16);
    }
    char* const base = r.arena().base;
    fm_assert(p[0] == base);
    fm_assert((uptr)(base + Tunables.bytes_until_superpages) % (2 << 20) == 0 && (uptr)base % (64 << 10) == 0);
    for (char* x : p)
        a.deallocate(x);
    fm_assert(a.num_allocated_buffers() == 0 && a.num_committed_slabs() == 1);
}

template<slab::tunables Tunables>
void test_buffers_on_both_sides_of_bytes_until_superpages()
{
    using R = slab::memory_resource<tag_bytes_until_superpages, Tunables>;
    static_assert(R::template pool_constants<6000, 16>.slab_bytes == 60000);
    constexpr u32 superpage_bytes = 2 << 20, bytes = 6000, slab_bytes = 60000;
    constexpr u32 boundary = Tunables.bytes_until_superpages == slab::tunables::no_superpages
                             ? 1 << 20 : Tunables.bytes_until_superpages;
    constexpr u32 n = (boundary + 2 * superpage_bytes) / slab_bytes * 10 + 20;
    R r;
    const slab::allocator<test_object<bytes>, R> a{&r};
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)(i / 10) * slab_bytes + (usize)(i % 10) * bytes);
        write_pattern(p[i], bytes, i);
    }
    const slab::detail::arena& arena = r.arena();
    fm_assert(p[0] == arena.base);
    fm_assert(p[boundary / slab_bytes * 10] < arena.base + boundary);
    fm_assert(p[boundary / slab_bytes * 10 + 9] + bytes > arena.base + boundary);
    for (u32 i = 0; i < n; i++)
        fm_assert(has_pattern(p[i], bytes, i));
    if (arena.bytes_until_superpages != slab::tunables::no_superpages)
        fm_assert(arena.num_committed_superpages == 3);
#ifdef _WIN32
    for (u32 i = 0; i < arena.num_committed_superpages; i++)
    {
        const char* const superpage = arena.base + boundary + (usize)i * superpage_bytes;
        fm_assert(memory_state(superpage) == MEM_COMMIT);
        fm_assert(memory_state(superpage + superpage_bytes - 1) == MEM_COMMIT);
    }
#endif
    for (char* x : p)
        a.deallocate(x);
    fm_assert(a.num_allocated_buffers() == 0);
}

template<slab::tunables Tunables>
void test_commits_past_bytes_until_superpages()
{
    using R = slab::memory_resource<tag_commits_past_bytes_until_superpages, Tunables>;
    constexpr u32 bytes = 65536, n = (1 << 20) / bytes + 1;
    R r;
    const slab::allocator<test_object<bytes>, R> a{&r};
    char* p[n];
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == p[0] + (usize)i * bytes);
    }
    const slab::detail::arena& arena = r.arena();
    const bool has_superpages = arena.bytes_until_superpages != slab::tunables::no_superpages;
    char* const past = arena.base + (1 << 20);
    fm_assert(p[n - 1] == past);
    if constexpr (Tunables.bytes_until_superpages == slab::tunables::no_superpages)
        fm_assert(!has_superpages && arena.num_committed_superpages == 0);
    else
        fm_assert(arena.num_committed_superpages == (has_superpages ? 1u : 0u));
#ifdef _WIN32
    constexpr u32 superpage_bytes = 2 << 20;
    if constexpr (Tunables.bytes_until_superpages != slab::tunables::no_superpages)
        fm_assert(has_superpages == (sizeof(void*) == 8 && windows_enable_large_pages().page == superpage_bytes));
    fm_assert(memory_state(past) == MEM_COMMIT && memory_state(past + bytes - 1) == MEM_COMMIT);
    const DWORD expected_state = has_superpages ? MEM_COMMIT : MEM_RESERVE;
    fm_assert(memory_state(past + bytes) == expected_state);
    fm_assert(memory_state(past + superpage_bytes - 1) == expected_state);
    fm_assert(memory_state(past + superpage_bytes) != MEM_COMMIT);
#endif
    write_pattern(past, bytes, 1);
    fm_assert(has_pattern(past, bytes, 1));
    for (char* x : p)
        a.deallocate(x);
    fm_assert(a.num_allocated_buffers() == 0);
}

#ifdef __linux__
struct smaps_fields
{
    bool has_nh_flag = false;
    u64 anon_huge_pages_kib = 0;
};

smaps_fields read_smaps_fields(const void* p)
{
    const auto address = (unsigned long)p;
    std::FILE* const f = std::fopen("/proc/self/smaps", "r");
    fm_assert(f);
    char line[4096];
    bool is_holding_address = false, is_address_mapped = false;
    unsigned long start = 0, end = 0, kib = 0;
    smaps_fields ret;
    while (std::fgets(line, sizeof line, f))
    {
        if (std::sscanf(line, "%lx-%lx", &start, &end) == 2)
        {
            is_holding_address = start <= address && address < end;
            is_address_mapped |= is_holding_address;
        }
        else if (is_holding_address && std::sscanf(line, "AnonHugePages: %lu kB", &kib) == 1)
            ret.anon_huge_pages_kib = kib;
        else if (is_holding_address && std::strncmp(line, "VmFlags:", 8) == 0)
            ret.has_nh_flag = std::strstr(line, " nh") != nullptr;
    }
    std::fclose(f);
    fm_assert(is_address_mapped);
    return ret;
}

bool has_transparent_huge_pages()
{
    std::FILE* const f = std::fopen("/sys/kernel/mm/transparent_hugepage/enabled", "r");
    if (f)
        std::fclose(f);
    return f != nullptr;
}

template<slab::tunables Tunables>
void test_arena_without_huge_pages_below_bytes_until_superpages()
{
    if (!has_transparent_huge_pages())
        return;
    using R = slab::memory_resource<tag_arena_without_huge_pages_below_bytes_until_superpages, Tunables>;
    constexpr u32 bytes = 65536, n = (4 << 20) / bytes;
    R r;
    const slab::allocator<test_object<bytes>, R> a{&r};
    char* p[n];
    for (u32 i = 0; i < n; i++)
        p[i] = allocate_bytes(a);
    for (u32 i = 0; i < n; i++)
        write_pattern(p[i], bytes, i);
    const slab::detail::arena& arena = r.arena();
    fm_assert(p[0] == arena.base);
    const smaps_fields fields = read_smaps_fields(arena.base);
    fm_assert(fields.has_nh_flag);
    fm_assert(fields.anon_huge_pages_kib == 0);
    if (arena.bytes_until_superpages != slab::tunables::no_superpages)
        fm_assert(!read_smaps_fields(arena.base + arena.bytes_until_superpages).has_nh_flag);
    for (u32 i = 0; i < n; i++)
        fm_assert(has_pattern(p[i], bytes, i));
    for (char* x : p)
        a.deallocate(x);
    fm_assert(a.num_allocated_buffers() == 0);
}
#endif

template<u32 Bytes, typename R>
void touch_buffers_of_new_slabs(R& r, u32 num_bytes, u32 thread)
{
    const slab::allocator<test_object<Bytes>, R> a{&r};
    const u32 n = num_bytes / Bytes;
    Array<char*> p{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        p[i][0] = (char)(i + thread);
        p[i][Bytes - 1] = (char)(i * 3 + thread);
    }
    for (u32 i = 0; i < n; i++)
    {
        fm_assert(p[i][0] == (char)(i + thread) && p[i][Bytes - 1] == (char)(i * 3 + thread));
        a.deallocate(p[i]);
    }
}

void test_threads_creating_slabs_past_bytes_until_superpages()
{
    using R = slab::memory_resource<tag_threads_creating_slabs_past_bytes_until_superpages,
                                   without_guard_pages(multi_threaded)>;
    constexpr u32 num_threads = 4, superpage_bytes = 2 << 20, bytes_per_thread = 8 << 20;
    for (u32 round = 0; round < 4; round++)
    {
        R r;
        run_threads<num_threads>([&](u32 t) {
            if (t == 0)
                touch_buffers_of_new_slabs<4096>(r, bytes_per_thread, t);
            else if (t == 1)
                touch_buffers_of_new_slabs<4112>(r, bytes_per_thread, t);
            else if (t == 2)
                touch_buffers_of_new_slabs<4128>(r, bytes_per_thread, t);
            else
                touch_buffers_of_new_slabs<4144>(r, bytes_per_thread, t);
        });
        const slab::detail::arena& arena = r.arena();
        const u64 end = (u64)arena.bump_offset * 16;
        fm_assert(arena.num_committed_superpages == arena.num_superpage_commits_started);
        if (arena.bytes_until_superpages == slab::tunables::no_superpages)
            fm_assert(arena.num_committed_superpages == 0);
        else
            fm_assert(arena.num_committed_superpages ==
                      (end - arena.bytes_until_superpages + superpage_bytes - 1) / superpage_bytes);
    }
}

constexpr slab::tunables small_reservation_tunables{.arena_reservation_bytes = 4 << 20,
                                                   .max_array_extent_bytes = 2 << 20,
                                                   .bytes_until_superpages = 1984 << 10,
                                                   .buffer_source = slab::buffer_source::slabs};
static_assert(((4 << 20) - small_reservation_tunables.bytes_until_superpages) % (2 << 20) != 0);

void test_small_reservation_filled_to_last_slab()
{
    using R = slab::memory_resource<tag_small_reservation_filled_to_last_slab, small_reservation_tunables>;
    constexpr u32 num_resources = 4, n = (4 << 20) / sizeof(u32_array<4>);
    R r[num_resources];
    for (u32 k = 0; k < num_resources; k++)
    {
        const slab::allocator<u32_array<4>, R> a{&r[k]};
        for (u32 i = 0; i < n; i++)
        {
            auto* const q = static_cast<u32_array<4>*>(a.allocate());
            fm_assert(reinterpret_cast<char*>(q) == r[k].arena().base + (usize)i * sizeof(u32_array<4>));
            *q = {{i, k, ~i, ~k}};
        }
        const slab::detail::arena& arena = r[k].arena();
        fm_assert(arena.bump_offset == arena.max_bump_offset);
        if (arena.bytes_until_superpages != slab::tunables::no_superpages)
            fm_assert(arena.num_committed_superpages == 2);
#ifdef _WIN32
        fm_assert(memory_state(arena.base + (4 << 20) - 1) == MEM_COMMIT);
#endif
    }
    for (u32 k = 0; k < num_resources; k++)
    {
        const slab::allocator<u32_array<4>, R> a{&r[k]};
        auto* const q = reinterpret_cast<u32_array<4>*>(r[k].arena().base);
        for (u32 i = 0; i < n; i++)
        {
            fm_assert(q[i].values[0] == i && q[i].values[1] == k && q[i].values[2] == ~i && q[i].values[3] == ~k);
            a.deallocate(&q[i]);
        }
        fm_assert(a.num_allocated_buffers() == 0);
    }
}

#ifdef _WIN32
bool set_lock_memory_privilege(bool is_enabled)
{
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;
    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = is_enabled ? SE_PRIVILEGE_ENABLED : 0;
    const bool ok = LookupPrivilegeValueA(nullptr, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid) &&
                    AdjustTokenPrivileges(token, FALSE, &tp, sizeof tp, nullptr, nullptr) &&
                    GetLastError() != ERROR_NOT_ALL_ASSIGNED;
    CloseHandle(token);
    return ok;
}

MEMORY_BASIC_INFORMATION memory_basic_information(const void* p)
{
    MEMORY_BASIC_INFORMATION info;
    fm_assert(VirtualQuery(p, &info, sizeof info) == sizeof info);
    return info;
}

// Returns true when a large page backs the page holding p, which the caller has written.
bool is_large_page(const void* p)
{
    PSAPI_WORKING_SET_EX_INFORMATION info = {};
    info.VirtualAddress = const_cast<void*>(p);
    fm_assert(QueryWorkingSetEx(GetCurrentProcess(), &info, sizeof info));
    return info.VirtualAttributes.Valid && info.VirtualAttributes.LargePage;
}

void test_superpages_without_lock_memory_privilege()
{
    constexpr u32 superpage_bytes = 2 << 20, bytes = 65536, n = (1 << 20) / bytes + superpage_bytes / bytes + 1;
    if (sizeof(void*) != 8 || windows_enable_large_pages().page != superpage_bytes)
        return;
    using R = slab::memory_resource<tag_superpages_without_lock_memory_privilege,
                                   without_guard_pages(single_threaded)>;
    char* regions[4];
    {
        R r;
        const slab::allocator<test_object<bytes>, R> a{&r};
        char* p[n];
        p[0] = allocate_bytes(a);
        const slab::detail::arena& arena = r.arena();
        fm_assert(arena.bytes_until_superpages == 1 << 20);
        fm_assert(set_lock_memory_privilege(false));
        for (u32 i = 1; i < n - 1; i++)
            p[i] = allocate_bytes(a);
        fm_assert(set_lock_memory_privilege(true));
        p[n - 1] = allocate_bytes(a);
        fm_assert(arena.num_committed_superpages == 2);
        char* const superpage = arena.base + (1 << 20);
        regions[0] = arena.memory.data();
        regions[1] = superpage;
        regions[2] = superpage + superpage_bytes;
        regions[3] = superpage + 2 * superpage_bytes;
        for (u32 i = 0; i < n; i++)
        {
            fm_assert(p[i] == p[0] + (usize)i * bytes);
            write_pattern(p[i], bytes, i);
        }
        fm_assert(memory_state(superpage) == MEM_COMMIT);
        fm_assert(memory_state(superpage + superpage_bytes - 1) == MEM_COMMIT);
        fm_assert(!is_large_page(superpage));
        fm_assert(!is_large_page(regions[2]));
        for (u32 i = 1; i < 3; i++)
        {
            const auto info = memory_basic_information(regions[i]);
            fm_assert(info.AllocationBase == regions[i] && info.State == MEM_COMMIT);
        }
        for (u32 i = 0; i < n; i++)
        {
            fm_assert(has_pattern(p[i], bytes, i));
            a.deallocate(p[i]);
        }
    }
    for (char* x : regions)
        fm_assert(memory_state(x) == MEM_FREE);
}
#endif

template<slab::tunables Tunables>
void test_allocator_and_resource_share_pools()
{
    using R = slab::memory_resource<tag_allocator_and_resource, Tunables>;
    R r;
    const slab::allocator<test_object<64>, R> a{&r};
    void* const p = a.allocate();
    void* const q = r.template allocate<64, 1>();
    void* const x = r.template allocate<64, 16>();
    fm_assert(a.num_allocated_buffers() == 3);
    fm_assert((r.template num_allocated_buffers<64, 16>() == 3));
    a.deallocate(q);
    r.template deallocate<64, 1>(x);
    r.template deallocate<64, 16>(p);
    fm_assert(a.num_allocated_buffers() == 0);

    R r2;
    void* const y = r2.template allocate<64, 16>();
    const slab::allocator<test_object<64>, R> b{&r2};
    void* const z = b.allocate();
    fm_assert(b.num_allocated_buffers() == 2);
    b.deallocate(y);
    r2.template deallocate<64, 16>(z);
    fm_assert(b.num_allocated_buffers() == 0);
}

// Sums 64 pools because a container's node size differs between standard libraries.
template<typename R>
u32 num_allocated_16_byte_aligned_buffers(R& r)
{
    return [&]<usize... I>(std::index_sequence<I...>) {
        return (0u + ... + r.template num_allocated_buffers<(I + 1) * 16, 16>());
    }(std::make_index_sequence<64>{});
}

struct alignas(16) key_value
{
    u64 key, value;
};

struct alignas(64) aligned_element { u64 values[8]; };

template<slab::tunables Tunables>
void test_std_allocator()
{
    using R = slab::memory_resource<tag_std_allocator, Tunables>;
    using A = slab::std_allocator<key_value, R>;
    using B = slab::std_allocator<u32, R>;
    using traits = std::allocator_traits<A>;
    static_assert(std::is_same_v<typename traits::template rebind_alloc<u32>, B>);
    static_assert(!traits::is_always_equal::value && !std::is_default_constructible_v<A>);
    static_assert(!traits::propagate_on_container_copy_assignment::value);
    static_assert(!traits::propagate_on_container_move_assignment::value);
    static_assert(!traits::propagate_on_container_swap::value);
    R r, other;
    A a{&r};
    const B b{a};
    const A c{&other};
    fm_assert(a == b && b == a && a != c && b.resource() == &r);
    fm_assert(traits::select_on_container_copy_construction(a).resource() == &r);

    const slab::allocator<key_value, R> s{&r};
    const slab::allocator<aligned_element, R> t{&r};
    constexpr bool has_slabs = R::tunables.buffer_source == slab::buffer_source::slabs;
    if constexpr (has_slabs)
        fm_assert(s.num_committed_slabs() == 0 && t.num_committed_slabs() == 0);
    key_value* const p = construct_at(a.allocate(1), key_value{83, 89});
    fm_assert(s.num_allocated_buffers() == 1 && t.num_allocated_buffers() == 0);
    slab::std_allocator<aligned_element, R> e{a};
    aligned_element* const q = e.allocate(1);
    fm_assert(t.num_allocated_buffers() == 1 && (uptr)q % 64 == 0);
    if constexpr (has_slabs)
        fm_assert(s.num_committed_slabs() == 1 && t.num_committed_slabs() == 1 &&
                  reinterpret_cast<char*>(q) == r.arena().base + 65536);
    e.deallocate(q, 1);

    key_value* const x = a.allocate(5);
    fm_assert((uptr)x % alignof(key_value) == 0 && s.num_allocated_buffers() == 1);
    for (u32 i = 0; i < 5; i++)
        construct_at(x + i, key_value{i * 97, i * 101});
    for (u32 i = 0; i < 5; i++)
        fm_assert(x[i].key == i * 97 && x[i].value == i * 101);
    a.deallocate(x, 5);
    fm_assert(p->key == 83 && p->value == 89);
    a.deallocate(p, 1);
    fm_assert(s.num_allocated_buffers() == 0 && t.num_allocated_buffers() == 0);
}

template<slab::tunables Tunables>
void test_std_containers()
{
    using R = slab::memory_resource<tag_std_containers, Tunables>;
    using allocator = slab::std_allocator<key_value, R>;
    constexpr u32 n = 1000;
    R r;
    {
        std::list<key_value, allocator> list{allocator{&r}};
        for (u32 i = 0; i < n; i++)
            list.push_back({i * 7919, i});
        fm_assert(Tunables.has_pool_per_type || num_allocated_16_byte_aligned_buffers(r) >= n);
        list.remove_if([](const key_value& x) { return x.value % 3 == 0; });
        u32 num_left = 0;
        u64 previous = 0;
        for (const key_value& x : list)
        {
            fm_assert(x.value % 3 != 0 && x.key == x.value * 7919 && x.value > previous);
            previous = x.value;
            num_left++;
        }
        fm_assert(num_left == n - (n + 2) / 3);
    }
    fm_assert(num_allocated_16_byte_aligned_buffers(r) == 0);
    {
        using map_allocator = slab::std_allocator<std::pair<const u64, u64>, R>;
        std::unordered_map<u64, u64, std::hash<u64>, std::equal_to<u64>, map_allocator> map{map_allocator{&r}};
        for (u32 i = 0; i < n; i++)
            map.emplace((u64)i * 7919 % 100003, i);
        fm_assert(map.size() == n && (Tunables.has_pool_per_type || num_allocated_16_byte_aligned_buffers(r) >= n));
        std::erase_if(map, [](const auto& x) { return x.second % 2 == 0; });
        fm_assert(map.size() == n / 2);
        for (u32 i = 0; i < n; i++)
        {
            const auto it = map.find((u64)i * 7919 % 100003);
            fm_assert(i % 2 == 0 ? it == map.end() : it != map.end() && it->second == i);
        }
    }
    fm_assert(num_allocated_16_byte_aligned_buffers(r) == 0);
    {
        const slab::allocator<aligned_element, R> v{&r};
        std::vector<aligned_element, slab::std_allocator<aligned_element, R>> vector{
            slab::std_allocator<aligned_element, R>{&r}};
        vector.reserve(1);
        fm_assert(v.num_allocated_buffers() == 1);
        for (u64 i = 0; i < 100; i++)
            vector.push_back({{i * 31, i * 37}});
        fm_assert((uptr)vector.data() % 64 == 0 && v.num_allocated_buffers() == 0);
        for (u64 i = 0; i < 100; i++)
            fm_assert(vector[i].values[0] == i * 31 && vector[i].values[1] == i * 37);
    }
    fm_assert(num_allocated_16_byte_aligned_buffers(r) == 0);
}

void test_destroying_after_lock_free_deallocation()
{
    using R = slab::memory_resource<tag_destroying_after_lock_free_deallocation,
                                   without_guard_pages(single_allocating_thread)>;
    R r;
    void* p[3];
    for (u32 i = 0; i < 3; i++)
        p[i] = r.allocate<64, 8>();
    for (u32 i = 0; i < 3; i++)
        r.deallocate<64, 8>(p[i]);
}

void test_alignment_above_4096()
{
    using R = slab::memory_resource<tag_aligned_8_kib, without_guard_pages(single_threaded)>;
    static_assert(has_num_committed_slabs<R, 8192, 8192>);
    R r;
    const slab::allocator<aligned_8_kib_object, R> a{&r};
    char* p[3];
    for (u32 i = 0; i < 3; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert((uptr)p[i] % 8192 == 0);
        p[i][8191] = (char)(i + 83);
    }
    for (u32 i = 0; i < 3; i++)
    {
        fm_assert(p[i][8191] == (char)(i + 83));
        a.deallocate(p[i]);
    }
    fm_assert(a.num_allocated_buffers() == 0 && a.num_committed_slabs() == 1);
}

template<slab::tunables Tunables>
void test_64_byte_alignment()
{
    using R = slab::memory_resource<tag_64_byte_alignment, Tunables>;
    using A = slab::std_allocator<aligned_element, R>;
    constexpr usize num_elements_above_largest_slab = Tunables.max_slab_bytes / sizeof(aligned_element) + 1;
    R r;
    // Its slab ends 48 bytes past a multiple of 64, so the next extent begins with padding.
    const slab::allocator<test_object<48>, R> b{&r};
    char* const x = allocate_bytes(b);
    const slab::allocator<aligned_element, R> s{&r};
    A a{&r};
    char* const p[] = {
        reinterpret_cast<char*>(a.allocate(100)),
        allocate_bytes(s),
        allocate_bytes(s),
        reinterpret_cast<char*>(a.allocate(1)),
        reinterpret_cast<char*>(a.allocate(8)),
        reinterpret_cast<char*>(a.allocate(num_elements_above_largest_slab)),
        static_cast<char*>(r.template allocate<64, 64>()),
        static_cast<char*>(r.template allocate<200, 64>()),
        static_cast<char*>(r.template allocate<70000, 64>()),
    };
    for (u32 i = 0; i < std::size(p); i++)
    {
        fm_assert((uptr)p[i] % 64 == 0);
        p[i][0] = (char)(i * 7 + 11);
        p[i][63] = (char)(i * 13 + 17);
    }
    for (u32 i = 0; i < std::size(p); i++)
        fm_assert(p[i][0] == (char)(i * 7 + 11) && p[i][63] == (char)(i * 13 + 17));
    fm_assert(s.num_allocated_buffers() == (Tunables.has_pool_per_type ? 3u : 4u));
    a.deallocate(reinterpret_cast<aligned_element*>(p[0]), 100);
    s.deallocate(p[1]);
    s.deallocate(p[2]);
    a.deallocate(reinterpret_cast<aligned_element*>(p[3]), 1);
    a.deallocate(reinterpret_cast<aligned_element*>(p[4]), 8);
    a.deallocate(reinterpret_cast<aligned_element*>(p[5]), num_elements_above_largest_slab);
    r.template deallocate<64, 64>(p[6]);
    r.template deallocate<200, 64>(p[7]);
    r.template deallocate<70000, 64>(p[8]);
    b.deallocate(x);
    fm_assert(s.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

void test_guard_pages_in_slab_allocator()
{
    constexpr slab::tunables tunables{.buffer_source = slab::buffer_source::guard_pages, .has_pool_per_type = false};
    using R = slab::memory_resource<tag_guard_pages, tunables>;
    static_assert(!has_num_committed_slabs<R, 48, 16>);
    R r;
    const slab::allocator<test_object<40>, R> a{&r};
    const usize page = virtual_memory::page_size();
    char* const p = allocate_bytes(a);
    char* const q = allocate_bytes(a);
    fm_assert((uptr)p % 16 == 0 && (uptr)(p + 48) % page == 0 && (uptr)(q + 48) % page == 0 && p != q);
#if fm_ASAN
    fm_assert(!is_poisoned(p) && !is_poisoned(p + 39) && is_poisoned(p + 40) && is_poisoned(p + 47));
#endif
    char* const s = static_cast<char*>(r.allocate<40, 8>());
    fm_assert((uptr)s % 16 == 0 && (uptr)(s + 48) % page == 0 && a.num_allocated_buffers() == 3);
    r.deallocate<40, 8>(s);
    a.deallocate(p);
#ifdef _WIN32
    fm_assert(memory_state(p) == MEM_FREE);
#endif
#if fm_ASAN
    fm_assert(!is_poisoned(q));
#endif
    fm_assert(a.num_allocated_buffers() == 1);
    a.deallocate(q);

    char* const x = static_cast<char*>(r.allocate<8192, 8192>());
    char* const y = static_cast<char*>(r.allocate<65552, 16>());
    char* const z = static_cast<char*>(r.allocate<70000, 16>());
    char* const w = static_cast<char*>(r.allocate<65537, 1>());
    fm_assert((uptr)x % 8192 == 0 && (uptr)(x + 8192) % page == 0 && r.num_allocated_buffers<8192, 8192>() == 1);
    fm_assert((uptr)(y + 65552) % page == 0 && (uptr)(z + 70000) % page == 0 && (uptr)w % 16 == 0);
#if fm_ASAN
    fm_assert(is_poisoned(y - 1) && is_poisoned(z - 1) && !is_poisoned(z) && !is_poisoned(z + 69999));
    fm_assert(is_poisoned(w - 1) && !is_poisoned(w) && !is_poisoned(w + 65536) && is_poisoned(w + 65537));
#endif
    r.deallocate<8192, 8192>(x);
    r.deallocate<65552, 16>(y);
    r.deallocate<70000, 16>(z);
    r.deallocate<65537, 1>(w);
#ifdef _WIN32
    fm_assert(memory_state(x) == MEM_FREE && memory_state(y) == MEM_FREE && memory_state(z) == MEM_FREE);
    fm_assert(memory_state(w) == MEM_FREE);
#endif

    slab::std_allocator<u64, R> c{&r};
    constexpr usize num_elements[] = {16, 450, 8750};
    for (usize n : num_elements)
    {
        char* const v = reinterpret_cast<char*>(c.allocate(n));
        fm_assert((uptr)(v + n * sizeof(u64)) % page == 0);
#if fm_ASAN
        fm_assert(!is_poisoned(v + n * sizeof(u64) - 1) && is_poisoned(v - 1));
#endif
        c.deallocate(reinterpret_cast<u64*>(v), n);
#ifdef _WIN32
        fm_assert(memory_state(v) == MEM_FREE);
#endif
    }
    fm_assert(a.num_allocated_buffers() == 0 && r.num_allocated_buffers<8192, 8192>() == 0 && !r.arena().base);
}

void test_operator_new_in_slab_allocator()
{
    constexpr slab::tunables tunables{.buffer_source = slab::buffer_source::operator_new, .has_pool_per_type = false};
    using R = slab::memory_resource<tag_operator_new, tunables>;
    static_assert(!has_num_committed_slabs<R, 48, 16>);
    R r;
    const slab::allocator<test_object<40>, R> a{&r};
    char* const p = allocate_bytes(a);
    char* const q = allocate_bytes(a);
    char* const s = static_cast<char*>(r.allocate<40, 8>());
    fm_assert((uptr)p % 16 == 0 && (uptr)q % 16 == 0 && (uptr)s % 16 == 0 && a.num_allocated_buffers() == 3);
#if fm_ASAN
    fm_assert(is_owned_by_asan_allocator(p) && is_owned_by_asan_allocator(s));
    fm_assert(!is_poisoned(p + 39) && is_poisoned(p + 40));
#endif
    r.deallocate<40, 8>(s);
    a.deallocate(p);
#if fm_ASAN
    fm_assert(!is_owned_by_asan_allocator(p) && is_poisoned(p));
#endif
    fm_assert(a.num_allocated_buffers() == 1);
    a.deallocate(q);

    char* const x = static_cast<char*>(r.allocate<8192, 8192>());
    char* const y = static_cast<char*>(r.allocate<69632, 4096>());
    char* const w = static_cast<char*>(r.allocate<65537, 1>());
    fm_assert((uptr)x % 8192 == 0 && (uptr)y % 4096 == 0 && (uptr)w % 16 == 0);
    fm_assert(r.num_allocated_buffers<8192, 8192>() == 1);
#if fm_ASAN
    fm_assert(is_owned_by_asan_allocator(x) && is_owned_by_asan_allocator(y) && is_owned_by_asan_allocator(w));
    fm_assert(!is_poisoned(w + 65536) && is_poisoned(w + 65537));
#endif
    r.deallocate<8192, 8192>(x);
    r.deallocate<69632, 4096>(y);
    r.deallocate<65537, 1>(w);

    slab::std_allocator<u64, R> c{&r};
    u64* const v = c.allocate(16);
    fm_assert((r.num_allocated_buffers<128, 16>() == 1));
    c.deallocate(v, 16);
    constexpr usize num_elements[] = {16, 450, 8750};
    for (usize n : num_elements)
    {
        char* const u = reinterpret_cast<char*>(c.allocate(n));
        fm_assert((uptr)u % 16 == 0);
#if fm_ASAN
        fm_assert(is_owned_by_asan_allocator(u));
        fm_assert(!is_poisoned(u + n * sizeof(u64) - 1) && is_poisoned(u + n * sizeof(u64)));
#endif
        c.deallocate(reinterpret_cast<u64*>(u), n);
    }
    fm_assert(a.num_allocated_buffers() == 0 && r.num_allocated_buffers<128, 16>() == 0);
    fm_assert(r.num_allocated_buffers<8192, 8192>() == 0 && !r.arena().base);
}

constexpr slab::tunables with_8_kib_slabs{.arena_reservation_bytes = usize{1} << 30, .max_slab_bytes = 8192,
                                         .buffer_source = slab::buffer_source::slabs,
                                         .is_empty_slab_given_back = true};

void test_slab_given_back_to_pool_of_other_size()
{
    using R = slab::memory_resource<tag_slab_given_back_to_pool_of_other_size, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<1024>, R> a{&r};
    const slab::allocator<stamped_buffer, R> b{&r};
    constexpr u32 per_a_slab = 64, per_b_slab = 1024, slab_bytes = 65536, n = 4 * per_a_slab;
    char* p[n];
    for (u32 i = 0; i < n; i++)
    {
        p[i] = allocate_bytes(a);
        write_pattern(p[i], 1024, i);
    }
    char* const base = r.arena().base;
    fm_assert(p[0] == base && p[n - 1] == base + 3 * slab_bytes + 63 * 1024);
    for (u32 i = 0; i < 3 * per_a_slab; i++)
        a.deallocate(p[i]);
    fm_assert(a.num_committed_slabs() == 2);
    const u32 bump_offset = r.arena().bump_offset;

    Array<stamped_buffer*> q{NoInit, 2 * per_b_slab};
    for (u32 i = 0; i < 2 * per_b_slab; i++)
    {
        q[i] = b.construct();
        fm_assert((char*)q[i] == base + slab_bytes + (usize)i * sizeof(stamped_buffer));
        write_stamp(*q[i], i);
    }
    fm_assert(r.arena().bump_offset == bump_offset && b.num_committed_slabs() == 2);
    for (u32 i = 0; i < per_a_slab; i++)
    {
        p[i] = allocate_bytes(a);
        fm_assert(p[i] == base + i * 1024);
        write_pattern(p[i], 1024, i);
    }
    for (u32 i = 0; i < 2 * per_b_slab; i++)
    {
        fm_assert(read_stamp(*q[i]) == i);
        b.destroy(q[i]);
    }
    for (u32 i = 0; i < n; i++)
        if (i < per_a_slab || i >= 3 * per_a_slab)
        {
            fm_assert(has_pattern(p[i], 1024, i));
            a.deallocate(p[i]);
        }
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

void test_freed_extent_merging_with_both_neighbours()
{
    using R = slab::memory_resource<tag_freed_extent_merging_with_both_neighbours,
                                   without_guard_pages(single_threaded)>;
    static_assert(R::pool_constants<40960, 1>.slab_bytes == 40960);
    static_assert(R::pool_constants<57344, 1>.slab_bytes == 57344);
    R r;
    const slab::allocator<test_object<40960>, R> a{&r};
    const slab::allocator<test_object<65536>, R> b{&r};
    const slab::allocator<test_object<57344>, R> c{&r};
    char* p[5];
    for (u32 i = 0; i < 5; i++)
        p[i] = allocate_bytes(a);
    char* const base = r.arena().base;
    fm_assert(p[0] == base && p[4] == base + 4 * 40960);
    a.deallocate(p[0]);
    a.deallocate(p[1]);
    a.deallocate(p[3]);
    a.deallocate(p[2]);
    fm_assert(a.num_committed_slabs() == 2);
    const u32 bump_offset = r.arena().bump_offset;

    char* const q = allocate_bytes(b);
    char* const s = allocate_bytes(c);
    fm_assert(q == base + 40960 && s == q + 65536 && r.arena().bump_offset == bump_offset);
    q[0] = 101;
    q[65535] = 103;
    s[0] = 107;
    s[57343] = 109;
    fm_assert(q[0] == 101 && q[65535] == 103 && s[0] == 107 && s[57343] == 109);
    b.deallocate(q);
    c.deallocate(s);
    a.deallocate(p[4]);
    fm_assert(a.num_allocated_buffers() == 0 && a.num_committed_slabs() == 1);
}

constexpr slab::tunables single_threaded_without_guard_pages(bool is_slab_of_last_deallocation_cached)
{
    slab::tunables t = without_guard_pages(single_threaded);
    t.is_slab_of_last_deallocation_cached = is_slab_of_last_deallocation_cached;
    return t;
}

template<bool IsSlabOfLastDeallocationCached>
void test_slab_index_reused_with_other_extent()
{
    using R = slab::memory_resource<tag_slab_index_reused_with_other_extent,
                                   single_threaded_without_guard_pages(IsSlabOfLastDeallocationCached)>;
    R r;
    const slab::allocator<test_object<40960>, R> a{&r};
    char* p[6];
    for (u32 i = 0; i < 6; i++)
        p[i] = allocate_bytes(a);
    fm_assert(p[0] == r.arena().base && p[5] == p[0] + 5 * 40960);
    a.deallocate(p[1]);
    const auto& s = *r.template pool_state<40960, 1, test_object<40960>>();
    if constexpr (IsSlabOfLastDeallocationCached)
        fm_assert(s.slab_of_last_deallocation == 1 && s.first_buffer_offset_of_last_deallocation == 40960 / 16);
    else
        fm_assert(s.slab_of_last_deallocation == (u32)-1 && s.first_buffer_offset_of_last_deallocation == (u32)-1);
    a.deallocate(p[2]);
    a.deallocate(p[4]);
    fm_assert(a.num_committed_slabs() == 4);

    char* const q1 = allocate_bytes(a);
    char* const q2 = allocate_bytes(a);
    char* const q3 = allocate_bytes(a);
    fm_assert(q1 == p[1] && q2 == p[4] && q3 == p[2]);
    a.deallocate(q2);
    char* const q4 = allocate_bytes(a);
    fm_assert(q4 == p[4]);
    for (char* q : {q1, q3, q4, p[0], p[3], p[5]})
        a.deallocate(q);
    fm_assert(a.num_allocated_buffers() == 0);
    if constexpr (!IsSlabOfLastDeallocationCached)
        fm_assert(s.slab_of_last_deallocation == (u32)-1 && s.first_buffer_offset_of_last_deallocation == (u32)-1);
}

void test_rest_below_4_kib_staying_inside_slab()
{
    using R = slab::memory_resource<tag_rest_below_4_kib_staying_inside_slab, without_guard_pages(single_threaded)>;
    R r;
    const slab::allocator<test_object<65536>, R> a{&r};
    const slab::allocator<test_object<48>, R> b{&r};
    constexpr u32 per_b_slab = 1365, slab_bytes = 65536;
    char* p[3];
    for (u32 i = 0; i < 3; i++)
        p[i] = allocate_bytes(a);
    char* const base = r.arena().base;
    a.deallocate(p[0]);
    a.deallocate(p[1]);

    Array<char*> q{NoInit, 2 * per_b_slab};
    for (u32 i = 0; i < 2 * per_b_slab; i++)
        q[i] = allocate_bytes(b);
    for (u32 i = 0; i < per_b_slab; i++)
        fm_assert(q[i] == base + slab_bytes + i * 48);
    fm_assert(q[per_b_slab] == base + 3 * slab_bytes);
    for (u32 i = 2 * per_b_slab; i-- > 0; )
        b.deallocate(q[i]);
    fm_assert(b.num_committed_slabs() == 1);
    const u32 bump_offset = r.arena().bump_offset;

    p[0] = allocate_bytes(a);
    p[1] = allocate_bytes(a);
    fm_assert(p[0] == base && p[1] == base + slab_bytes && r.arena().bump_offset == bump_offset);
    for (char* x : p)
        a.deallocate(x);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

void test_pools_taking_back_their_given_back_slabs()
{
    using R = slab::memory_resource<tag_pools_taking_back_their_given_back_slabs, without_guard_pages(single_threaded)>;
    constexpr u32 per_a_slab = 1365, per_b_slab = 1024, slab_bytes = 65536;
    R r;
    const slab::allocator<test_object<48>, R> a{&r};
    const slab::allocator<test_object<64>, R> b{&r};
    Array<char*> p{NoInit, 2 * per_a_slab}, q{NoInit, 2 * per_b_slab};
    for (u32 k = 0; k < 2; k++)
    {
        for (u32 i = k * per_a_slab; i < (k + 1) * per_a_slab; i++)
            p[i] = allocate_bytes(a);
        for (u32 i = k * per_b_slab; i < (k + 1) * per_b_slab; i++)
            q[i] = allocate_bytes(b);
    }
    // A 48-byte slab spans 65520 bytes, so the 64-byte slab after it starts 16 bytes past its end.
    char* const base = r.arena().base;
    fm_assert(p[0] == base && q[0] == base + slab_bytes);
    fm_assert(p[per_a_slab] == base + 2 * slab_bytes && q[per_b_slab] == base + 3 * slab_bytes);
    const u32 bump_offset = r.arena().bump_offset;

    for (char* x : q)
        b.deallocate(x);
    fm_assert(b.num_committed_slabs() == 1);
    for (u32 i = 0; i < 2 * per_b_slab; i++)
        q[i] = allocate_bytes(b);
    fm_assert(q[per_b_slab] == base + 3 * slab_bytes && r.arena().bump_offset == bump_offset);

    for (char* x : p)
        a.deallocate(x);
    fm_assert(a.num_committed_slabs() == 1);
    for (u32 i = 0; i < 2 * per_a_slab; i++)
        p[i] = allocate_bytes(a);
    fm_assert(p[per_a_slab] == base + 2 * slab_bytes && r.arena().bump_offset == bump_offset);

    for (char* x : p)
        a.deallocate(x);
    for (char* x : q)
        b.deallocate(x);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

void test_64_byte_pool_slab_after_48_byte_pool_slab()
{
    using R = slab::memory_resource<tag_64_byte_pool_slab_after_48_byte_pool_slab,
                                   without_guard_pages(single_threaded)>;
    constexpr u32 per_b_slab = 1024;
    R r;
    const slab::allocator<test_object<48>, R> a{&r};
    const slab::allocator<test_object<64>, R> b{&r};
    char* const x = allocate_bytes(a);
    write_pattern(x, 48, per_b_slab);
    Array<char*> q{NoInit, per_b_slab};
    for (u32 i = 0; i < per_b_slab; i++)
    {
        q[i] = allocate_bytes(b);
        write_pattern(q[i], 64, i);
    }
    fm_assert(x == r.arena().base && q[0] == x + 65536 && (uptr)q[0] % 64 == 0);
    for (u32 i = 0; i < per_b_slab; i++)
        fm_assert(q[i] == q[0] + i * 64 && has_pattern(q[i], 64, i));
    fm_assert(has_pattern(x, 48, per_b_slab));
    for (char* y : q)
        b.deallocate(y);
    a.deallocate(x);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

void test_4096_aligned_pool_taking_padding_from_free_extent()
{
    using R = slab::memory_resource<tag_4096_aligned_pool_taking_padding_from_free_extent, with_8_kib_slabs>;
    static_assert(R::pool_constants<4112, 1>.slab_bytes == 4112);
    static_assert(R::pool_constants<4096, 4096>.slab_bytes == 8192);
    R r;
    const slab::allocator<test_object<4112>, R> a{&r};
    const slab::allocator<object_aligned_4096, R> b{&r};
    constexpr u32 a_bytes = 4112;
    char* p[6];
    for (u32 i = 0; i < 6; i++)
        p[i] = allocate_bytes(a);
    char* const base = r.arena().base;
    fm_assert(p[5] == base + 5 * a_bytes);
    a.deallocate(p[4]);
    for (u32 i = 1; i <= 3; i++)
        a.deallocate(p[i]);
    const u32 bump_offset = r.arena().bump_offset;

    char* q[4];
    q[0] = allocate_bytes(b);
    q[1] = allocate_bytes(b);
    fm_assert(q[0] == base + 8192 && q[1] == base + 12288 && r.arena().bump_offset == bump_offset);
    q[2] = allocate_bytes(b);
    q[3] = allocate_bytes(b);
    fm_assert(q[2] == base + 28672);
    for (u32 i = 4; i-- > 0; )
        b.deallocate(q[i]);

    p[4] = allocate_bytes(a);
    p[1] = allocate_bytes(a);
    p[2] = allocate_bytes(a);
    fm_assert(p[4] == base + 4 * a_bytes && p[1] == base + a_bytes && p[2] == base + 2 * a_bytes);
    for (u32 i : {0u, 1u, 2u, 4u, 5u})
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

void test_8192_aligned_pool_leaving_front_of_free_extent()
{
    using R = slab::memory_resource<tag_8192_aligned_pool_leaving_front_of_free_extent, with_8_kib_slabs>;
    static_assert(R::pool_constants<8192, 8192>.slab_bytes == 8192);
    R r;
    const slab::allocator<test_object<4112>, R> a{&r};
    const slab::allocator<aligned_8_kib_object, R> b{&r};
    constexpr u32 a_bytes = 4112;
    char* p[8];
    for (u32 i = 0; i < 8; i++)
        p[i] = allocate_bytes(a);
    char* const base = r.arena().base;
    a.deallocate(p[6]);
    for (u32 i = 2; i <= 5; i++)
        a.deallocate(p[i]);
    const u32 bump_offset = r.arena().bump_offset;

    char* const q = allocate_bytes(b);
    fm_assert(q == base + 16384);
    p[6] = allocate_bytes(a);
    p[2] = allocate_bytes(a);
    fm_assert(p[6] == base + 6 * a_bytes && p[2] == base + 2 * a_bytes && r.arena().bump_offset == bump_offset);

    char* const s = allocate_bytes(b);
    p[3] = allocate_bytes(a);
    fm_assert(s == base + 40960 && p[3] == base + 8 * a_bytes);
    b.deallocate(q);
    b.deallocate(s);
    for (u32 i : {0u, 1u, 2u, 3u, 6u, 7u})
        a.deallocate(p[i]);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0);
}

void test_free_extents_staying_in_their_resource()
{
    using R = slab::memory_resource<tag_free_extents_staying_in_their_resource, without_guard_pages(single_threaded)>;
    constexpr u32 slab_bytes = 65536;
    R r1, r2;
    const slab::allocator<test_object<65536>, R> a1{&r1}, a2{&r2};
    const slab::allocator<test_object<48>, R> b1{&r1}, b2{&r2};
    char* p[3];
    for (char*& x : p)
        x = allocate_bytes(a1);
    char* const x0 = allocate_bytes(a2);
    char* const x1 = allocate_bytes(a2);
    a1.deallocate(p[0]);
    a1.deallocate(p[1]);

    char* const q2 = allocate_bytes(b2);
    char* const q1 = allocate_bytes(b1);
    fm_assert(q2 == r2.arena().base + 2 * slab_bytes && q1 == r1.arena().base + slab_bytes);
    b1.deallocate(q1);
    b2.deallocate(q2);
    a1.deallocate(p[2]);
    a2.deallocate(x0);
    a2.deallocate(x1);
    fm_assert(a1.num_allocated_buffers() == 0 && a2.num_allocated_buffers() == 0);
}

constexpr u32 array_size_classes[] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640, 768, 896, 1024,
    1280, 1536, 1792, 2048, 2560, 3072, 3584,
};

constexpr bool has_jemalloc_size_classes()
{
    u32 bytes = 0;
    for (u32 size_class : array_size_classes)
        for (; bytes <= size_class; bytes++)
            if (slab::detail::array_size_class_bytes(bytes, 1) != size_class)
                return false;
    return slab::detail::max_array_size_class_bytes == 3584;
}

template<typename R, usize Align>
constexpr bool has_array_size_class_indexes()
{
    for (u32 bytes = 0; bytes <= slab::detail::max_array_size_class_bytes; bytes++)
        if (R::template array_size_class_index<Align>(bytes) !=
            R::size_class_index(slab::detail::array_size_class_bytes(bytes, Align), Align))
            return false;
    return true;
}

static_assert(has_array_size_class_indexes<default_resource, 1>());
static_assert(has_array_size_class_indexes<default_resource, 16>());
static_assert(has_array_size_class_indexes<slab::memory_resource<tag_size_classes, with_8_kib_slabs>, 1>());
static_assert(has_array_size_class_indexes<slab::memory_resource<tag_size_classes, with_8_kib_slabs>, 16>());

void test_arrays_at_size_class_boundaries()
{
    using R = slab::memory_resource<tag_arrays_at_size_class_boundaries, without_guard_pages(single_threaded)>;
    static_assert(has_jemalloc_size_classes());
    static_assert(slab::detail::array_size_class_bytes(100, 64) == 128);
    static_assert(slab::detail::array_size_class_bytes(129, 4096) == 4096);
    R r;
    slab::std_allocator<char, R> a{&r};
    char* const p[] = {a.allocate(128), a.allocate(129), a.allocate(3584)};
    fm_assert((r.num_allocated_buffers<128, 16>() == 1 && r.num_allocated_buffers<160, 16>() == 1 &&
               r.num_allocated_buffers<3584, 16>() == 1));
    const slab::detail::arena& arena = r.arena();
    constexpr u32 bytes_per_pagemap_entry = slab::detail::arena::bytes_per_pagemap_entry;
    const u32 bump_offset = arena.bump_offset;
    char* const q = a.allocate(3585);
    char* const s = a.allocate(3585);
    const usize offset = (usize)(q - arena.base);
    fm_assert(offset == (usize)bump_offset * 16 && s == q + bytes_per_pagemap_entry);
    fm_assert((usize)arena.bump_offset * 16 == offset + 2 * bytes_per_pagemap_entry);
    fm_assert(arena.pagemap[offset / bytes_per_pagemap_entry + 1] >> slab::detail::arena::num_extent_start_bits ==
              slab::detail::arena::array_extent_marker);
    fm_assert((r.num_allocated_buffers<3584, 16>() == 1 && r.num_allocated_buffers<4096, 16>() == 0));
    write_pattern(q, 3585, 1);
    write_pattern(s, 3585, 2);
    for (u32 i = 0; i < 3; i++)
        write_pattern(p[i], i == 0 ? 128 : i == 1 ? 129 : 3584, i + 3);
    fm_assert(has_pattern(q, 3585, 1) && has_pattern(s, 3585, 2));
    a.deallocate(q, 3585);
    a.deallocate(s, 3585);
    fm_assert(arena.num_uncoalesced_array_extents == 2 && a.allocate(3585) == s);
    a.deallocate(s, 3585);
    fm_assert(has_pattern(p[0], 128, 3) && has_pattern(p[1], 129, 4) && has_pattern(p[2], 3584, 5));
    a.deallocate(p[0], 128);
    a.deallocate(p[1], 129);
    a.deallocate(p[2], 3584);
    fm_assert((r.num_allocated_buffers<128, 16>() == 0 && r.num_allocated_buffers<160, 16>() == 0 &&
               r.num_allocated_buffers<3584, 16>() == 0));
}

template<slab::thread_model Model>
void test_arrays_of_each_size_class_through_std_allocator()
{
    using R = slab::memory_resource<tag_arrays_of_each_size_class_through_std_allocator, without_guard_pages(Model)>;
    constexpr u32 num_classes = std::size(array_size_classes), n = 6;
    // Odd i takes the smallest size of class c.
    // For c = 0 that is 2, since std_allocator allocates a single char as an object.
    auto array_bytes = [](u32 c, u32 i) -> u32 {
        return i % 2 == 0 ? array_size_classes[c] : c == 0 ? 2 : array_size_classes[c - 1] + 1;
    };
    auto has_num_allocated_buffers_in_each_class = [](R& r, u32 num) {
        return [&]<usize... I>(std::index_sequence<I...>) {
            return ((r.template num_allocated_buffers<array_size_classes[I], 16>() == num) && ...);
        }(std::make_index_sequence<num_classes>{});
    };
    R r;
    slab::std_allocator<char, R> a{&r};
    char* p[num_classes][n];
    for (u32 c = 0; c < num_classes; c++)
        for (u32 i = 0; i < n; i++)
        {
            p[c][i] = a.allocate(array_bytes(c, i));
            write_pattern(p[c][i], array_bytes(c, i), c * n + i);
        }
    for (u32 c = 0; c < num_classes; c++)
        for (u32 i = 0; i < n; i++)
            fm_assert(p[c][i] == p[c][0] + i * array_size_classes[c]);
    fm_assert(has_num_allocated_buffers_in_each_class(r, n));
    for (u32 c = num_classes; c-- > 0; )
        for (u32 i = 0; i < n; i += 2)
        {
            fm_assert(has_pattern(p[c][i], array_bytes(c, i), c * n + i));
            a.deallocate(p[c][i], array_bytes(c, i));
        }
    fm_assert(has_num_allocated_buffers_in_each_class(r, n / 2));
    for (u32 c = 0; c < num_classes; c++)
        for (u32 i = 1; i < n; i += 2)
        {
            fm_assert(has_pattern(p[c][i], array_bytes(c, i), c * n + i));
            a.deallocate(p[c][i], array_bytes(c, i));
        }
    fm_assert(has_num_allocated_buffers_in_each_class(r, 0));
}

template<slab::tunables Tunables>
void test_arrays_of_two_sizes_sharing_pool()
{
    using R = slab::memory_resource<tag_arrays_of_two_sizes_sharing_pool, Tunables>;
    R r;
    slab::std_allocator<char, R> a{&r};
    slab::std_allocator<u32, R> b{&r};
    char* const p = a.allocate(130);
    char* const q = reinterpret_cast<char*>(b.allocate(40));
    char* const s = static_cast<char*>(r.template allocate<150, 8>());
    fm_assert((r.template num_allocated_buffers<160, 16>() == 3));
    if constexpr (Tunables.buffer_source == slab::buffer_source::slabs)
        fm_assert(q == p + 160 && s == q + 160 && r.template num_committed_slabs<160, 16>() == 1);
    write_pattern(p, 130, 1);
    write_pattern(q, 160, 2);
    write_pattern(s, 150, 3);
    fm_assert(has_pattern(p, 130, 1) && has_pattern(q, 160, 2) && has_pattern(s, 150, 3));
    a.deallocate(p, 130);
    fm_assert((r.template num_allocated_buffers<160, 16>() == 2));
    b.deallocate(reinterpret_cast<u32*>(q), 40);
    r.template deallocate<150, 8>(s);
    fm_assert((r.template num_allocated_buffers<160, 16>() == 0));
}

void test_freed_array_extent_merging_and_serving_slab()
{
    using R = slab::memory_resource<tag_freed_array_extent_merging_and_serving_slab,
                                   without_guard_pages(single_threaded)>;
    R r;
    slab::std_allocator<char, R> a{&r};
    const slab::allocator<test_object<64>, R> b{&r};
    constexpr u32 array_bytes = 24576;
    char* p[4];
    for (char*& x : p)
        x = a.allocate(array_bytes);
    const slab::detail::arena& arena = r.arena();
    char* const base = arena.base;
    fm_assert(p[0] == base && p[3] == base + 3 * array_bytes);
    a.deallocate(p[0], array_bytes);
    a.deallocate(p[2], array_bytes);
    a.deallocate(p[1], array_bytes);
    fm_assert(arena.num_uncoalesced_array_extents == 3);
    const u32 bump_offset = arena.bump_offset;

    char* const q = allocate_bytes(b);
    char* const s = a.allocate(8192);
    fm_assert(q == base && s == base + 65536 && arena.bump_offset == bump_offset);
    write_pattern(q, 64, 1);
    write_pattern(s, 8192, 2);
    fm_assert(has_pattern(q, 64, 1) && has_pattern(s, 8192, 2));
    b.deallocate(q);
    a.deallocate(s, 8192);
    a.deallocate(p[3], array_bytes);
    fm_assert(b.num_allocated_buffers() == 0);
}

template<slab::thread_model Model>
void test_arena_counting_allocated_array_extents()
{
    using R = slab::memory_resource<tag_arena_counting_allocated_array_extents, without_guard_pages(Model)>;
    R r;
    slab::std_allocator<char, R> a{&r};
    const slab::allocator<test_object<64>, R> b{&r};
    const slab::detail::arena& arena = r.arena();
    char* const p = a.allocate(8192);
    char* const x = allocate_bytes(b);
    char* const q = a.allocate(3584);
    constexpr usize mapped_bytes = R::tunables.max_array_extent_bytes + 16;
    char* const s = a.allocate(mapped_bytes);
    fm_assert(p == arena.base && arena.num_allocated_array_extents == 1);
    a.deallocate(p, 8192);
    fm_assert(arena.num_allocated_array_extents == 0);
    const u32 bump_offset = arena.bump_offset;
    char* const t = a.allocate(4096);
    char* const u = a.allocate(5000);
    fm_assert(t == arena.base && arena.bump_offset > bump_offset && arena.num_allocated_array_extents == 2);
    a.deallocate(t, 4096);
    fm_assert(arena.num_allocated_array_extents == 1);
    b.deallocate(x);
    a.deallocate(q, 3584);
    a.deallocate(s, mapped_bytes);
    a.deallocate(u, 5000);
}

template<slab::thread_model Model>
void test_array_extent_pending_until_next_extent()
{
    using R = slab::memory_resource<tag_array_extent_pending_until_next_extent, without_guard_pages(Model)>;
    constexpr u32 bytes = 8192, bytes_per_pagemap_entry = slab::detail::arena::bytes_per_pagemap_entry;
    R r;
    slab::std_allocator<char, R> a{&r};
    const slab::detail::arena& arena = r.arena();
    char* const p = a.allocate(bytes);
    char* const q = a.allocate(bytes);
    const usize offset = (usize)(p - arena.base);
    a.deallocate(p, bytes);
    for (usize u = offset / bytes_per_pagemap_entry; u < (offset + bytes) / bytes_per_pagemap_entry; u++)
        fm_assert(arena.pagemap[u] >> slab::detail::arena::num_extent_start_bits ==
                  slab::detail::arena::array_extent_marker);
    fm_assert(arena.first_pending_array_extent == offset / 16 && arena.num_allocated_array_extents == 1);
    const u32 bump_offset = arena.bump_offset;
    char* const s = a.allocate(bytes);
    fm_assert(s == p && arena.bump_offset == bump_offset && arena.first_pending_array_extent == (u32)-1);
    write_pattern(s, bytes, 1);
    write_pattern(q, bytes, 2);
    fm_assert(has_pattern(s, bytes, 1) && has_pattern(q, bytes, 2));
    a.deallocate(s, bytes);
    a.deallocate(q, bytes);
}

void test_free_extent_of_more_than_4_mib()
{
    using R = slab::memory_resource<tag_free_extent_of_more_than_4_mib, without_guard_pages(single_threaded)>;
    constexpr u32 num_arrays = 80, array_bytes = 65536;
    R r;
    slab::std_allocator<char, R> a{&r};
    const slab::allocator<test_object<64>, R> b{&r};
    char* p[num_arrays];
    for (char*& x : p)
        x = a.allocate(array_bytes);
    const slab::detail::arena& arena = r.arena();
    char* const base = arena.base;
    fm_assert(p[0] == base && p[num_arrays - 1] == base + (num_arrays - 1) * array_bytes);
    for (char* x : p)
        a.deallocate(x, array_bytes);
    const u32 bump_offset = arena.bump_offset;
    char* const q = a.allocate(8192);
    char* const s = allocate_bytes(b);
    fm_assert(q == base && s == base + 8192 && arena.bump_offset == bump_offset);
    write_pattern(q, 8192, 1);
    write_pattern(s, 64, 2);
    fm_assert(has_pattern(q, 8192, 1) && has_pattern(s, 64, 2));
    b.deallocate(s);
    a.deallocate(q, 8192);
    fm_assert(b.num_allocated_buffers() == 0);
}

void test_2_mib_array_at_8192_alignment_from_free_extent()
{
    using R = slab::memory_resource<tag_2_mib_array_at_8192_alignment,
                                   slab::tunables{.max_slab_bytes = 2 << 20,
                                                 .buffer_source = slab::buffer_source::slabs,
                                                 .is_empty_slab_given_back = true}>;
    constexpr u32 array_bytes = 2 << 20;
    R r;
    const slab::detail::arena& arena = r.arena();
    char* const p = static_cast<char*>(r.allocate_array<16>(array_bytes));
    char* const q = static_cast<char*>(r.allocate_array<16>(array_bytes));
    fm_assert(p == arena.base && q == p + array_bytes);
    r.deallocate_array<16>(p, array_bytes);
    r.deallocate_array<16>(q, array_bytes);
    const u32 bump_offset = arena.bump_offset;
    char* const s = static_cast<char*>(r.allocate_array<8192>(array_bytes));
    fm_assert(s == q && arena.bump_offset == bump_offset);
    write_pattern(s, array_bytes, 1);
    fm_assert(has_pattern(s, array_bytes, 1));
    r.deallocate_array<8192>(s, array_bytes);
}

void test_arrays_above_largest_slab()
{
    using R = slab::memory_resource<tag_arrays_above_largest_slab, without_guard_pages(single_threaded)>;
    constexpr u32 slab_bytes = R::tunables.max_slab_bytes, extent_bytes = R::tunables.max_array_extent_bytes;
    static_assert(slab_bytes < extent_bytes && extent_bytes == 32 << 20);
    constexpr u32 sizes[] = {slab_bytes + 16, extent_bytes, extent_bytes + 16};
    R r;
    const slab::detail::arena& arena = r.arena();
    slab::std_allocator<char, R> a{&r};
    auto is_in_arena = [&arena](const char* p) {
        return arena.base && (uptr)p - (uptr)arena.base < (usize)arena.bump_offset * 16;
    };
    char* p[std::size(sizes)];
    for (u32 i = 0; i < std::size(sizes); i++)
    {
        p[i] = a.allocate(sizes[i]);
        write_pattern(p[i], sizes[i], i + 1);
    }
    fm_assert(is_in_arena(p[0]) && is_in_arena(p[1]) && !is_in_arena(p[2]));
    fm_assert(arena.num_allocated_array_extents == 2);
    for (u32 i = 0; i < std::size(sizes); i++)
        fm_assert(has_pattern(p[i], sizes[i], i + 1));
    a.deallocate(p[2], sizes[2]);
#ifdef _WIN32
    fm_assert(memory_state(p[2]) == MEM_FREE);
#endif
    a.deallocate(p[1], sizes[1]);
    a.deallocate(p[0], sizes[0]);
    fm_assert(arena.num_allocated_array_extents == 0);
}

template<slab::thread_model Model>
void test_freed_1_mib_array_extent_reused()
{
    using R = slab::memory_resource<tag_freed_1_mib_array_extent_reused, without_guard_pages(Model)>;
    constexpr u32 bytes = 1 << 20;
    static_assert(bytes > R::tunables.max_slab_bytes);
    R r;
    const slab::detail::arena& arena = r.arena();
    slab::std_allocator<char, R> a{&r};
    char* const p = a.allocate(bytes);
    const u32 bump_offset = arena.bump_offset;
    fm_assert(p == arena.base);
    a.deallocate(p, bytes);
    char* const q = a.allocate(bytes);
    fm_assert(q == p && arena.bump_offset == bump_offset && arena.num_allocated_array_extents == 1);
    write_pattern(q, bytes, 1);
    fm_assert(has_pattern(q, bytes, 1));
    a.deallocate(q, bytes);
}

template<slab::tunables Tunables>
void test_vector_growth_keeping_elements()
{
    using R = slab::memory_resource<tag_vector_growth_keeping_elements, Tunables>;
    using A = slab::std_allocator<u64, R>;
    constexpr u32 n = 20000;
    static_assert(n * sizeof(u64) > Tunables.max_slab_bytes);
    R r;
    {
        std::vector<u64, A> v{A{&r}};
        usize capacity = 0;
        u32 num_reallocations = 0;
        for (u32 i = 0; i < n; i++)
        {
            v.push_back((u64)i * 2654435761);
            if (v.capacity() != capacity)
            {
                capacity = v.capacity();
                num_reallocations++;
                for (u32 j = 0; j <= i; j++)
                    fm_assert(v[j] == (u64)j * 2654435761);
            }
        }
        fm_assert(num_reallocations >= 15);
    }
    fm_assert(num_allocated_16_byte_aligned_buffers(r) == 0);
}

template<slab::tunables Tunables>
void test_pools_of_types_with_one_rounded_size()
{
    using R = slab::memory_resource<tag_pools_of_types_with_one_rounded_size, Tunables>;
    constexpr bool has_pool_per_type = Tunables.has_pool_per_type;
    R r;
    const slab::allocator<same_size_a, R> a{&r};
    const slab::allocator<same_size_b, R> b{&r};
    const slab::allocator<test_object<96>, R> c{&r};
    slab::std_allocator<same_size_a, R> arrays{&r};
    slab::std_allocator<same_size_b, R> arrays_b{&r};
    same_size_a* const x = a.construct();
    same_size_b* const y = b.construct();
    char* const z = allocate_bytes(c);
    same_size_a* const w = arrays.allocate(2);
    same_size_b* const v = arrays_b.allocate(2);
    if constexpr (has_pool_per_type)
        fm_assert(a.num_allocated_buffers() == 1 && b.num_allocated_buffers() == 1 && c.num_allocated_buffers() == 1);
    else
        fm_assert(a.num_allocated_buffers() == 2 && b.num_allocated_buffers() == 2 && c.num_allocated_buffers() == 3);
    fm_assert(r.template num_allocated_buffers<48, 16>() == (has_pool_per_type ? 0u : 2u));
    fm_assert(r.template num_allocated_buffers<96, 16>() == (has_pool_per_type ? 2u : 3u));
    if constexpr (Tunables.buffer_source == slab::buffer_source::slabs)
        fm_assert(reinterpret_cast<const char*>(v) == reinterpret_cast<const char*>(w) + 96);
    write_pattern(reinterpret_cast<char*>(w), 96, 7);
    write_pattern(reinterpret_cast<char*>(v), 96, 11);
    fm_assert(has_pattern(reinterpret_cast<const char*>(w), 96, 7));
    arrays_b.deallocate(v, 2);
    fm_assert(has_pattern(reinterpret_cast<const char*>(w), 96, 7));
    arrays.deallocate(w, 2);
    c.deallocate(z);
    b.destroy(y);
    a.destroy(x);
    fm_assert(a.num_allocated_buffers() == 0 && b.num_allocated_buffers() == 0 && c.num_allocated_buffers() == 0);
    fm_assert(r.template num_allocated_buffers<96, 16>() == 0);
}

template<u32... I>
void test_pool_per_type_index_used_by_second_resource(std::integer_sequence<u32, I...>)
{
    using R = slab::memory_resource<tag_pool_per_type_index_used_by_second_resource,
                                   with_pool_per_type(single_threaded)>;
    constexpr u32 last = sizeof...(I) - 1;
    using L = distinct_16_byte_object<last>;
    R r1, r2;
    void* const p[] = {r1.allocate<16, 8, distinct_16_byte_object<I>>()...};
    fm_assert(slab::detail::pool_per_type_index<L> >= 8);
    fm_assert(((r1.num_allocated_buffers<16, 8, distinct_16_byte_object<I>>() == 1) && ...));
    void* const v = r2.allocate<16, 8>();
    fm_assert(((r2.num_allocated_buffers<16, 8, distinct_16_byte_object<I>>() == 0) && ...));
    void* const q = r2.allocate<16, 8, L>();
    fm_assert(((r2.num_allocated_buffers<16, 8, distinct_16_byte_object<I>>() == (I == last ? 1u : 0u)) && ...));
    fm_assert(r1.num_allocated_buffers<16, 8>() == 0 && r2.num_allocated_buffers<16, 8>() == 1);
    r2.deallocate<16, 8, L>(q);
    r2.deallocate<16, 8>(v);
    (r1.deallocate<16, 8, distinct_16_byte_object<I>>(p[I]), ...);
    fm_assert(((r1.num_allocated_buffers<16, 8, distinct_16_byte_object<I>>() == 0) && ...));
}

template<typename T, typename R>
void check_std_allocator_objects(R& r, u32 serial)
{
    constexpr u32 n = 67;
    slab::std_allocator<T, R> a{&r};
    const slab::allocator<T, R> s{&r};
    T* p[n];
    for (u32 i = 0; i < n; i++)
    {
        p[i] = a.allocate(1);
        write_pattern(reinterpret_cast<char*>(p[i]), sizeof(T), serial * n + i);
    }
    fm_assert(s.num_allocated_buffers() == n);
    for (u32 i = 0; i < n; i += 2)
        a.deallocate(p[i], 1);
    fm_assert(s.num_allocated_buffers() == n / 2);
    for (u32 i = 1; i < n; i += 2)
        fm_assert(has_pattern(reinterpret_cast<const char*>(p[i]), sizeof(T), serial * n + i));
    for (u32 i = 0; i < n; i += 2)
    {
        p[i] = a.allocate(1);
        write_pattern(reinterpret_cast<char*>(p[i]), sizeof(T), serial * n + i);
    }
    for (u32 i = 0; i < n; i++)
        fm_assert(has_pattern(reinterpret_cast<const char*>(p[i]), sizeof(T), serial * n + i));
    for (u32 i = n; i-- > 0; )
        a.deallocate(p[i], 1);
    fm_assert(s.num_allocated_buffers() == 0);
}

template<slab::thread_model Model, usize... I>
void test_std_allocator_objects_at_each_bitmap_word_count(std::index_sequence<I...>)
{
    using R = slab::memory_resource<tag_std_allocator_objects_at_each_bitmap_word_count, with_pool_per_type(Model)>;
    static_assert(((R::template pool_constants<(usize)16 << I, 1>.num_bitmap_words_per_slab == 64u >> I) && ...));
    R r;
    (check_std_allocator_objects<test_object<(usize)16 << I>>(r, (u32)I), ...);
}

template<slab::tunables Tunables>
void test_buffer_index_from_multiplier()
{
    for (u32 bytes = 16; bytes <= Tunables.max_slab_bytes; bytes += 16)
    {
        const slab::detail::pool_constants P{bytes, 16, Tunables};
        for (u32 i = 0; i < P.num_buffers_per_slab; i++)
            fm_assert((u32)((u64{i} * P.buffer_bytes * P.buffer_index_multiplier) >> 32) == i);
    }
}

} // namespace

void Test::test_slab_allocator()
{
    test_ascending_order();
    test_slab_boundary();
    test_freed_subset();
    test_pool_keeps_one_empty_slab();
    test_pool_keeps_empty_slabs_until_arena_grows();
#if defined _WIN32 || defined __linux__
    // With a 32-bit target's 64 MiB arena, this pool's slab bitmaps lie in the pool object's page.
    if constexpr (sizeof(void*) == 8)
        test_given_back_slab_bitmap_committed_at_first_give_back();
#endif
    test_pool_shared_by_rounded_size();
    test_alignment_and_large_types();
    test_construct_and_destroy();
    test_poisoning();
    test_slabs_sharing_words();
    test_nonfull_slab_bitmap_growth<single_threaded>();
    test_nonfull_slab_bitmap_growth<single_allocating_thread>();
    test_nonfull_slab_bitmap_growth<multi_threaded>();
    test_freed_subset_after_header_growth<single_threaded>(2200000, 300000);
    test_freed_subset_after_header_growth<single_allocating_thread>(2200000, 300000);
    test_slabs_of_two_pools_end_to_end();
    test_deallocation_at_boundary_inside_unit<single_threaded>();
    test_deallocation_at_boundary_inside_unit<single_allocating_thread>();
    test_deallocation_at_boundary_inside_unit<multi_threaded>();
    test_aligned_pools_after_16_aligned_slab();
    test_merge_of_several_slabs<single_threaded>();
    test_merge_of_several_slabs<single_allocating_thread>();
    test_merge_of_several_slabs<multi_threaded>();
    test_deallocation_far_below_lowest_nonfull_slab<single_threaded>();
    test_deallocation_far_below_lowest_nonfull_slab<single_allocating_thread>();
    test_deallocation_far_below_lowest_nonfull_slab<multi_threaded>();
    test_slabs_of_one_word_and_of_4096_buffers<single_threaded>();
    test_slabs_of_one_word_and_of_4096_buffers<single_allocating_thread>();
    test_slabs_of_one_word_and_of_4096_buffers<multi_threaded>();
    test_large_buffers();
    test_resource_objects();
    test_arena_base_alignment<without_guard_pages(single_threaded)>();
    test_arena_base_alignment<slab::tunables{.bytes_until_superpages = 192 << 10,
                                            .buffer_source = slab::buffer_source::slabs,
                                            .is_empty_slab_given_back = true}>();
    test_arena_base_alignment<slab::tunables{.arena_reservation_bytes = usize{1} << 30, .max_slab_bytes = 8192,
                                            .buffer_source = slab::buffer_source::slabs,
                                            .is_empty_slab_given_back = true}>();
    test_buffers_on_both_sides_of_bytes_until_superpages<without_guard_pages(single_threaded)>();
    test_buffers_on_both_sides_of_bytes_until_superpages<without_guard_pages(multi_threaded)>();
    test_buffers_on_both_sides_of_bytes_until_superpages<without_guard_pages_or_superpages(single_threaded)>();
    test_threads_creating_slabs_past_bytes_until_superpages();
    test_commits_past_bytes_until_superpages<without_guard_pages(single_threaded)>();
    test_commits_past_bytes_until_superpages<without_guard_pages_or_superpages(single_threaded)>();
#ifdef __linux__
    test_arena_without_huge_pages_below_bytes_until_superpages<without_guard_pages(single_threaded)>();
    test_arena_without_huge_pages_below_bytes_until_superpages<without_guard_pages_or_superpages(single_threaded)>();
#endif
    test_small_reservation_filled_to_last_slab();
#ifdef _WIN32
    test_superpages_without_lock_memory_privilege();
#endif
    test_allocator_and_resource_share_pools<slab::tunables{.has_pool_per_type = false}>();
    test_allocator_and_resource_share_pools<without_pool_per_type(single_threaded)>();
    test_std_allocator<slab::tunables{}>();
    test_std_allocator<without_guard_pages(single_threaded)>();
    test_std_containers<slab::tunables{}>();
    test_std_containers<slab::tunables{.has_pool_per_type = false}>();
    test_std_containers<without_guard_pages(single_threaded)>();
    test_std_containers<without_pool_per_type(single_threaded)>();
    test_destroying_after_lock_free_deallocation();
    test_alignment_above_4096();
    test_64_byte_alignment<without_guard_pages(single_threaded)>();
    test_64_byte_alignment<without_pool_per_type(single_threaded)>();
    test_64_byte_alignment<slab::tunables{.buffer_source = slab::buffer_source::guard_pages}>();
    test_64_byte_alignment<slab::tunables{.buffer_source = slab::buffer_source::operator_new}>();
    test_64_byte_alignment<with_arrays_mapped_above_largest_slab>();
    test_guard_pages_in_slab_allocator();
    test_operator_new_in_slab_allocator();
    test_slab_given_back_to_pool_of_other_size();
    test_freed_extent_merging_with_both_neighbours();
    test_slab_index_reused_with_other_extent<true>();
    test_slab_index_reused_with_other_extent<false>();
    test_rest_below_4_kib_staying_inside_slab();
    test_pools_taking_back_their_given_back_slabs();
    test_64_byte_pool_slab_after_48_byte_pool_slab();
    test_4096_aligned_pool_taking_padding_from_free_extent();
    test_8192_aligned_pool_leaving_front_of_free_extent();
    test_free_extents_staying_in_their_resource();
    test_arrays_at_size_class_boundaries();
    test_arrays_of_each_size_class_through_std_allocator<single_threaded>();
    test_arrays_of_each_size_class_through_std_allocator<single_allocating_thread>();
    test_arrays_of_two_sizes_sharing_pool<without_guard_pages(single_threaded)>();
    test_arrays_of_two_sizes_sharing_pool<slab::tunables{.buffer_source = slab::buffer_source::guard_pages}>();
    test_arrays_of_two_sizes_sharing_pool<slab::tunables{.buffer_source = slab::buffer_source::operator_new}>();
    test_freed_array_extent_merging_and_serving_slab();
    test_arena_counting_allocated_array_extents<single_threaded>();
    test_arena_counting_allocated_array_extents<single_allocating_thread>();
    test_arena_counting_allocated_array_extents<multi_threaded>();
    test_array_extent_pending_until_next_extent<single_allocating_thread>();
    test_array_extent_pending_until_next_extent<multi_threaded>();
    test_free_extent_of_more_than_4_mib();
    test_2_mib_array_at_8192_alignment_from_free_extent();
    test_arrays_above_largest_slab();
    test_freed_1_mib_array_extent_reused<single_threaded>();
    test_freed_1_mib_array_extent_reused<single_allocating_thread>();
    test_freed_1_mib_array_extent_reused<multi_threaded>();
    test_vector_growth_keeping_elements<slab::tunables{}>();
    test_vector_growth_keeping_elements<without_guard_pages(single_threaded)>();
    test_vector_growth_keeping_elements<with_arrays_mapped_above_largest_slab>();
    test_pools_of_types_with_one_rounded_size<without_pool_per_type(single_threaded)>();
    test_pools_of_types_with_one_rounded_size<with_pool_per_type(single_threaded)>();
    test_pools_of_types_with_one_rounded_size<with_pool_per_type(multi_threaded)>();
    test_pools_of_types_with_one_rounded_size<with_pool_per_type(single_threaded, slab::buffer_source::guard_pages)>();
    test_pool_per_type_index_used_by_second_resource(std::make_integer_sequence<u32, 16>{});
    test_std_allocator_objects_at_each_bitmap_word_count<single_threaded>(std::make_index_sequence<7>{});
    test_std_allocator_objects_at_each_bitmap_word_count<single_allocating_thread>(std::make_index_sequence<7>{});
    test_buffer_index_from_multiplier<slab::tunables{}>();
    test_buffer_index_from_multiplier<slab::tunables{.max_slab_bytes = 2 << 20}>();
    test_slab_global_std_allocator();
}

} // namespace floormat
