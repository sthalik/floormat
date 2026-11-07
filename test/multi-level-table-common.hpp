#pragma once
#include "compat/combinations.hpp"
#include "compat/defs.hpp"
#include "compat/enumerate.hpp"
#include "compat/multi-level-table.inl"
#include "compat/limits.hpp"
#include "random/random.hpp"
#include <array>
#include <cmath>
#include <type_traits>
#include <utility>
#include <gtl/phmap.hpp>
#include <mg/Functions.h>
#if fm_ASAN
#include <cstring>
#include <sanitizer/asan_interface.h>
#endif

namespace floormat {

template<mlt_params P>
struct mlt_coord_traits<std::array<u32, 3>, P>
{
    static constexpr mlt_coords coords(const std::array<u32, 3>& c) noexcept { return {{ c[0], c[1], c[2] }}; }
};

} // namespace floormat

namespace floormat::mlt_test {

struct tracked
{
    static constexpr u64 act_flag = u64{1} << 63;
    static constexpr u32 key_bits = 40;
    static u32 live;
    static void (*hook)(u32 action, u64 key);
    static void (*on_destroy)(u64 v);

    u64 v = 0;

    tracked() noexcept = default;
    explicit tracked(u64 v) noexcept : v{v} { live++; }
    tracked(tracked&& other) noexcept : v{std::exchange(other.v, 0)} {}
    tracked& operator=(tracked&& other) noexcept
    {
        reset();
        v = std::exchange(other.v, 0);
        return *this;
    }
    ~tracked() noexcept { reset(); }
    explicit operator bool() const noexcept { return v != 0; }

    void reset() noexcept
    {
        if (!v)
            return;
        const auto x = std::exchange(v, 0);
        live--;
        if (on_destroy)
            on_destroy(x);
        if (x & act_flag)
            hook(u32(x >> 61) & 3, x & ((u64{1} << key_bits) - 1));
    }
};

void test_bit_tables();
void test_plane_tables();
void test_space_tables();

enum : u32 { act_erase, act_insert, act_clear, };

static_assert(!std::is_copy_constructible_v<tracked> && !std::is_copy_assignable_v<tracked>);

// A value whose destructor calls tracked::hook. Bits 61..62 hold the action, 0..39 the key.
// The tag in between keeps values unique.
constexpr u64 act(u32 action, u64 key, u64 tag = 0)
{
    fm_assert(!(key >> tracked::key_bits) && !(tag >> (61 - tracked::key_bits)));
    return tracked::act_flag | u64{action} << 61 | tag << tracked::key_bits | key;
}

template<typename Table> Table* target = nullptr;

template<typename Table>
void act_on(u32 action, u64 key)
{
    Table& t = *target<Table>;
    switch (action)
    {
    case act_erase:
        (void)t.find(key);
        (void)t.erase(key);
        break;
    case act_insert:
        fm_assert(t.insert(key, tracked{key + 1}));
        break;
    case act_clear:
        t.clear();
        break;
    }
}

template<typename Table>
void set_target(Table& t)
{
    target<Table> = &t;
    tracked::hook = act_on<Table>;
}

constexpr inline mlt_params chunk_like_params = mlt_params{
    .levels = { {.bits = 14}, {.bits = 4}, {.bits = 18, .dynamic = true}, },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
}.validate();

constexpr inline mlt_params id_like_params = mlt_params{
    .levels = { {.bits = 17}, {.bits = 18, .dynamic = true}, },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
    .free_empty = true,
}.validate();

constexpr inline mlt_params split_params = mlt_params{
    .levels = { {.bits = 3}, {.bits = 2, .inline_zero = true}, {.bits = 4, .dynamic = true}, },
}.validate();

constexpr inline mlt_params flat_params = mlt_params{
    .levels = { {.bits = 6}, },
}.validate();

constexpr inline mlt_params deep_params = mlt_params{
    .levels = { {.bits = 2}, {.bits = 3}, {.bits = 2}, {.bits = 5, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params byte_params = mlt_params{
    .levels = { {.bits = 17}, {.bits = 21, .dynamic = true}, },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
    .free_empty = true,
}.validate();

constexpr inline mlt_params short_params = mlt_params{
    .levels = { {.bits = 18}, {.bits = 20, .dynamic = true}, },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
}.validate();

constexpr inline mlt_params no_pages_params = mlt_params{
    .levels = { {.bits = 18}, },
    .top_source = mlt_source::superpage,
}.validate();

constexpr inline mlt_params one_split_params = mlt_params{
    .levels = { {.bits = 3, .inline_zero = true}, {.bits = 4, .dynamic = true}, },
}.validate();

constexpr inline mlt_params one_entry_params = mlt_params{
    .levels = { {.bits = 5, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params wide_params = mlt_params{
    .levels = { {.bits = 21}, {.bits = 22, .inline_zero = true}, {.bits = 21, .dynamic = true}, },
    .page_source = mlt_source::superpage,
}.validate();

constexpr inline mlt_params bit_page_params = mlt_params{
    .levels = { {.bits = 1, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params bit_flat_params = mlt_params{
    .levels = { {.bits = 1}, },
}.validate();

constexpr inline mlt_params bit_pair_params = mlt_params{
    .levels = { {.bits = 1}, {.bits = 1, .dynamic = true}, },
}.validate();

constexpr inline mlt_params bit_split_params = mlt_params{
    .levels = { {.bits = 1, .inline_zero = true}, {.bits = 1, .dynamic = true}, },
}.validate();

constexpr inline mlt_params bit_triple_params = mlt_params{
    .levels = { {.bits = 1}, {.bits = 1, .inline_zero = true}, {.bits = 1, .dynamic = true}, },
}.validate();

constexpr inline mlt_params bit_deep_params = mlt_params{
    .levels = { {.bits = 1}, {.bits = 1}, {.bits = 1}, {.bits = 1, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params wide_top_params = mlt_params{
    .levels = { {.bits = 4}, {.bits = 1, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params narrow_top_params = mlt_params{
    .levels = { {.bits = 1}, {.bits = 4, .dynamic = true}, },
}.validate();

constexpr inline mlt_params bit_zero_params = mlt_params{
    .levels = { {.bits = 3}, {.bits = 1, .inline_zero = true}, {.bits = 3, .dynamic = true}, },
}.validate();

constexpr inline mlt_params wide_zero_params = mlt_params{
    .levels = { {.bits = 1}, {.bits = 3, .inline_zero = true}, {.bits = 1, .dynamic = true}, },
}.validate();

constexpr inline mlt_params zero_top_params = mlt_params{
    .levels = { {.bits = 4, .inline_zero = true}, {.bits = 1, .dynamic = true}, },
}.validate();

constexpr inline mlt_params split_free_params = mlt_params{
    .levels = { {.bits = 3}, {.bits = 2, .inline_zero = true}, {.bits = 4, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params bit_split_free_params = mlt_params{
    .levels = { {.bits = 1}, {.bits = 1, .inline_zero = true}, {.bits = 1, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params plane_params = mlt_params{
    .levels = { {.bits = {2, 1, 0}}, {.bits = {3, 1, 0}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params plane_deep_free_params = mlt_params{
    .levels = { {.bits = {3, 0, 0}}, {.bits = {0, 2, 0}}, {.bits = {2, 1, 0}, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params plane_no_y_params = mlt_params{
    .levels = { {.bits = {3, 0, 0}}, {.bits = {2, 0, 0}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params plane_zero_params = mlt_params{
    .levels = { {.bits = {1, 1, 0}}, {.bits = {1, 0, 0}, .inline_zero = true}, {.bits = {2, 1, 0}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params plane_zero_free_params = mlt_params{
    .levels = { {.bits = {1, 1, 0}}, {.bits = {1, 0, 0}, .inline_zero = true}, {.bits = {2, 1, 0}, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params plane_no_pages_params = mlt_params{
    .levels = { {.bits = {3, 2, 0}}, },
}.validate();

constexpr inline mlt_params plane_wide_params = mlt_params{
    .levels = { {.bits = {16, 0, 0}}, {.bits = {16, 1, 0}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params space_free_params = mlt_params{
    .levels = { {.bits = {1, 1, 1}}, {.bits = {2, 1, 0}, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params space_deep_params = mlt_params{
    .levels = { {.bits = {2, 2, 0}}, {.bits = {0, 0, 2}}, {.bits = {3, 3, 0}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params space_zero_free_params = mlt_params{
    .levels = { {.bits = {1, 1, 0}}, {.bits = {0, 0, 1}, .inline_zero = true}, {.bits = {2, 2, 1}, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params space_no_pages_params = mlt_params{
    .levels = { {.bits = {1, 2, 1}}, },
}.validate();

constexpr inline mlt_params space_one_entry_params = mlt_params{
    .levels = { {.bits = {1, 0, 1}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params plane_large_params = mlt_params{
    .levels = { {.bits = {2, 2, 0}}, {.bits = {1, 1, 0}, .inline_zero = true}, {.bits = {3, 3, 0}, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params space_large_params = mlt_params{
    .levels = { {.bits = {1, 1, 1}}, {.bits = {1, 1, 0}, .inline_zero = true}, {.bits = {2, 2, 2}, .dynamic = true}, },
    .free_empty = true,
}.validate();

using coord3 = std::array<u32, 3>;

template<typename Table> constexpr inline mlt_params params_of = {};
template<typename T, mlt_params P> constexpr inline mlt_params params_of<multi_level_table<T, P>> = P;

// Key bit -> the coordinate and coordinate bit it holds. Built from the top of the key down,
// apart from the table's pack(), which builds from the bottom up.
template<mlt_params P>
struct key_layout
{
    u8 dim[64] = {}, bit[64] = {};

    consteval key_layout()
    {
        u32 below[3] = { P.dim_bits(0), P.dim_bits(1), P.dim_bits(2) };
        u32 b = P.key_bits();
        for (u32 i = 0; i < P.depth(); i++)
        {
            const mlt_bits& level = P.levels[i].bits;
            for (u32 d = level.dims; d-- > 0; )
            {
                below[d] -= level.dim[d];
                for (u32 j = level.dim[d]; j-- > 0; )
                {
                    b--;
                    dim[b] = u8(d);
                    bit[b] = u8(below[d] + j);
                }
            }
        }
        fm_assert(b == 0);
    }
};

template<mlt_params P> constexpr inline key_layout<P> layout_of{};

template<mlt_params P>
constexpr u64 test_pack(coord3 c)
{
    constexpr u32 key_bits = P.key_bits();
    u64 k = 0;
    for (u32 b = 0; b < key_bits; b++)
        k |= u64{c[layout_of<P>.dim[b]] >> layout_of<P>.bit[b] & 1} << b;
    return k;
}

template<mlt_params P>
constexpr coord3 test_unpack(u64 k)
{
    constexpr u32 key_bits = P.key_bits();
    coord3 c = {};
    for (u32 b = 0; b < key_bits; b++)
        c[layout_of<P>.dim[b]] |= u32(k >> b & 1) << layout_of<P>.bit[b];
    return c;
}

consteval coord3 coord_bits(mlt_params P)
{
    return { P.dim_bits(0), P.dim_bits(1), P.dim_bits(2) };
}

consteval coord3 box_bits(mlt_params P)
{
    if (P.has_pages())
    {
        const mlt_bits& b = P.levels[P.depth() - 1].bits;
        return { b.dim[0], b.dim[1], b.dim[2] };
    }
    return coord_bits(P);
}

template<typename Table, typename F>
decltype(auto) at_coords(u64 k, F&& f)
{
    const coord3 c = test_unpack<params_of<Table>>(k);
    fm_assert(test_pack<params_of<Table>>(c) == k);
    fm_assert(Table::pack(c) == k);
    return f(c);
}

template<typename Table>
constexpr bool packs_to(coord3 c, u64 k)
{
    return Table::pack(c) == k && test_pack<params_of<Table>>(c) == k && test_unpack<params_of<Table>>(k) == c;
}

using chunk_like_table = multi_level_table<u64, chunk_like_params>;
using id_like_table = multi_level_table<tracked, id_like_params>;
using split_table = multi_level_table<u32, split_params>;
using flat_table = multi_level_table<u16, flat_params>;
using deep_table = multi_level_table<u64, deep_params>;
using deep_tracked_table = multi_level_table<tracked, deep_params>;
using no_pages_table = multi_level_table<tracked, no_pages_params>;
using one_split_table = multi_level_table<tracked, one_split_params>;
using one_entry_table = multi_level_table<u64, one_entry_params>;
using wide_table = multi_level_table<u8, wide_params>;
using bit_page_table = multi_level_table<tracked, bit_page_params>;
using bit_flat_table = multi_level_table<tracked, bit_flat_params>;
using bit_flat_int_table = multi_level_table<u32, bit_flat_params>;
using bit_pair_table = multi_level_table<tracked, bit_pair_params>;
using bit_split_table = multi_level_table<tracked, bit_split_params>;
using bit_triple_table = multi_level_table<tracked, bit_triple_params>;
using bit_deep_table = multi_level_table<tracked, bit_deep_params>;
using wide_top_table = multi_level_table<tracked, wide_top_params>;
using narrow_top_table = multi_level_table<tracked, narrow_top_params>;
using bit_zero_table = multi_level_table<tracked, bit_zero_params>;
using wide_zero_table = multi_level_table<tracked, wide_zero_params>;
using zero_top_table = multi_level_table<tracked, zero_top_params>;
using split_free_table = multi_level_table<tracked, split_free_params>;
using bit_split_free_table = multi_level_table<tracked, bit_split_free_params>;
using plane_table = multi_level_table<tracked, plane_params>;
using plane_deep_free_table = multi_level_table<tracked, plane_deep_free_params>;
using plane_no_y_table = multi_level_table<tracked, plane_no_y_params>;
using plane_zero_table = multi_level_table<tracked, plane_zero_params>;
using plane_zero_free_table = multi_level_table<tracked, plane_zero_free_params>;
using plane_no_pages_table = multi_level_table<tracked, plane_no_pages_params>;
using plane_wide_table = multi_level_table<u8, plane_wide_params>;
using space_free_table = multi_level_table<tracked, space_free_params>;
using space_deep_table = multi_level_table<tracked, space_deep_params>;
using space_deep_int_table = multi_level_table<u32, space_deep_params>;
using space_zero_free_table = multi_level_table<tracked, space_zero_free_params>;
using space_no_pages_table = multi_level_table<tracked, space_no_pages_params>;
using space_one_entry_table = multi_level_table<tracked, space_one_entry_params>;
using plane_large_table = multi_level_table<tracked, plane_large_params>;
using space_large_table = multi_level_table<tracked, space_large_params>;

template<typename Table>
constexpr bool move_only = !std::is_copy_constructible_v<Table> && !std::is_copy_assignable_v<Table> &&
                           std::is_nothrow_move_constructible_v<Table> && std::is_nothrow_move_assignable_v<Table>;

template<typename Table>
constexpr usize top_bytes = sizeof(typename decltype(std::declval<const Table&>().raw_top())::Type) << Table::top_bits;

template<typename Table>
bool present(const Table& t, u64 key)
{
    const auto* s = t.find(key);
    return s && *s;
}

template<u32 zero_bits, typename Top>
const auto& ref_at(const Top& top, u64 ti, u64 zi)
{
    if constexpr (zero_bits > 0)
    {
        if (!zi)
            return top[ti].zero;
        fm_assert(top[ti].side);
        return top[ti].side[zi];
    }
    else
    {
        fm_assert(!zi);
        return top[ti];
    }
}

template<typename T, mlt_params P, typename Visit>
u64 check_layers(const multi_level_table<T, P>& t, Visit&& visit)
{
    using Table = multi_level_table<T, P>;
    const auto top = t.raw_top();
    u64 total = 0;

    if constexpr (!Table::has_pages)
    {
        for (const auto& [k, slot] : enumerate(top))
            if (detail_mlt::occupied(slot))
            {
                visit(u64{k}, slot);
                total++;
            }
    }
    else
    {
        constexpr u32 page_bits = Table::page_bits, zero_bits = Table::zero_bits;
        constexpr u32 page_size = 1u << page_bits, zero_mask = (1u << zero_bits) - 1;

        const auto pages = t.raw_pages();
        for (const auto& [a, b] : combinations(pages))
            fm_assert(a.page != b.page);
        for (const auto& rec : pages)
        {
            fm_assert(rec.page && rec.page != t.raw_spare().page);
            fm_assert(detail_mlt::page_of(ref_at<zero_bits>(top, rec.top_index, rec.zero_index)) == rec.page);
            const u64 base = (u64{rec.top_index} << zero_bits | rec.zero_index) << page_bits;
            u32 n = 0;
            for (u32 k = 0; k < page_size; k++)
                if (detail_mlt::occupied(rec.page[k]))
                {
                    visit(base | k, rec.page[k]);
                    n++;
                }
            if constexpr (P.free_empty)
                fm_assert(n == ref_at<zero_bits>(top, rec.top_index, rec.zero_index).live);
            total += n;
        }
        if constexpr (P.free_empty)
            fm_assert(total == t.size());

        // with the checks above, every page in the top is in the directory once
        u32 num_refs = 0;
        for (const auto& e : top)
        {
            if constexpr (zero_bits > 0)
            {
                if (detail_mlt::page_of(e.zero))
                    num_refs++;
                if (e.side)
                {
                    fm_assert(!detail_mlt::page_of(e.side[0]));
                    u32 side_refs = 0;
                    for (u32 z = 1; z <= zero_mask; z++)
                        if (detail_mlt::page_of(e.side[z]))
                            side_refs++;
                    if constexpr (P.free_empty)
                        fm_assert(side_refs > 0 && side_refs == e.side[0].live);
                    num_refs += side_refs;
                }
            }
            else if (detail_mlt::page_of(e))
                num_refs++;
        }
        fm_assert(num_refs == pages.size());

        if (const T* spare = t.raw_spare().page)
            for (u32 k = 0; k < page_size; k++)
                fm_assert(!detail_mlt::occupied(spare[k]));
    }
    return total;
}

#if fm_ASAN
template<typename U>
void check_heap_array(const U* p, u32 n)
{
    const usize bytes = usize{n} * sizeof(U);
    fm_assert(!__asan_region_is_poisoned(const_cast<U*>(p), bytes));
    // p + bytes can be the next chunk's first byte, which ASan doesn't always poison.
    // new[] puts an array cookie before p when U has a non-trivial destructor.
    void* begin = nullptr;
    usize size = 0;
    const char* kind = __asan_locate_address(const_cast<U*>(p), nullptr, 0, &begin, &size);
    fm_assert(kind && !std::strcmp(kind, "heap"));
    fm_assert(static_cast<const char*>(begin) + size == reinterpret_cast<const char*>(p) + bytes);
}

template<typename T, mlt_params P>
void check_heap_layers(const multi_level_table<T, P>& t)
{
    using Table = multi_level_table<T, P>;
    const auto top = t.raw_top();
    if constexpr (P.top_source == mlt_source::heap)
        check_heap_array(top.data(), 1u << Table::top_bits);
    if constexpr (Table::has_pages)
    {
        if constexpr (Table::zero_bits > 0)
            for (const auto& e : top)
                if (e.side)
                    check_heap_array(e.side, 1u << Table::zero_bits);
        if constexpr (P.page_source == mlt_source::heap)
        {
            for (const auto& rec : t.raw_pages())
                check_heap_array(rec.page, 1u << Table::page_bits);
            if (const T* spare = t.raw_spare().page)
                check_heap_array(spare, 1u << Table::page_bits);
        }
    }
}
#endif

template<typename T, mlt_params P>
void check_layout(const multi_level_table<T, P>& t, usize top_size_bytes, ArrayView<const u64> keys)
{
    using Table = multi_level_table<T, P>;
    const auto top = t.raw_top();
    fm_assert(top.size() == 1u << Table::top_bits);
    fm_assert(sizeof(top[0]) << Table::top_bits == top_size_bytes);
    if constexpr (P.top_source == mlt_source::superpage)
        fm_assert(t.raw_top_alloc().ptr == top.data() && t.raw_top_alloc().size == top_size_bytes);
    else
        fm_assert(!t.raw_top_alloc().ptr);

    (void)check_layers(t, [](u64, const T&) {});
#if fm_ASAN
    check_heap_layers(t);
#endif

    if constexpr (!Table::has_pages)
    {
        for (u64 k : keys)
            fm_assert(t.find(k) == &top[k] && top[k]);
    }
    else
    {
        constexpr u32 page_bits = Table::page_bits, zero_bits = Table::zero_bits;
        constexpr u64 page_mask = (u64{1} << page_bits) - 1, zero_mask = (u64{1} << zero_bits) - 1;
        for (u64 k : keys)
        {
            const T* s = t.find(k);
            fm_assert(s && *s);
            const T* page = detail_mlt::page_of(ref_at<zero_bits>(top, k >> (page_bits + zero_bits), (k >> page_bits) & zero_mask));
            fm_assert(s == page + (k & page_mask));
        }
    }
}

enum class key_mode : u8 { uniform, walk, };

enum class walk_type : u8
{
    none, simple, levy_flight, persistent, kinetic_growth, true_self_avoiding, loop_erased, vertex_reinforced,
    elephant, resetting, random_environment, excited, langton_ant, rotor_router, internal_dla, eden, retrace,
    page_corner_drift,
};

struct random_params
{
    u64 seed = 0;
    u32 ops = 0;
    u32 top_size_bytes = 0;
    key_mode mode = key_mode::uniform;
    walk_type walk = walk_type::none;
    u32 walkers = 1;
    u32 windows = 0, window_size = 0;
    bool hooks = false;
    u32 check_every = 1024, clear_every = 20000, move_every = 5000;
    u32 live_cap = (u32)-1, page_cap = (u32)-1;
};

inline u32 below(Random::xoshiro256starstar& rng, u32 n)
{
    return u32((Random::next(rng) >> 32) * n >> 32);
}

constexpr u64 low_mask(u32 bits)
{
    return bits ? ~u64{0} >> (64 - bits) : 0;
}

constexpr u64 mix64(u64 x)
{
    x = (x ^ x >> 30) * 0xbf58476d1ce4e5b9;
    x = (x ^ x >> 27) * 0x94d049bb133111eb;
    return x ^ x >> 31;
}

// Values are unique, so a value stored under the wrong key fails the comparison with the hash map.
template<typename Table>
class random_run
{
    using T = std::remove_cvref_t<decltype(*std::declval<const Table&>().find(0))>;
    static constexpr bool is_tracked = std::is_same_v<T, tracked>;
    static constexpr bool free_empty = requires (const Table& t) { t.size(); };
    static constexpr u32 page_bits = Table::page_bits, zero_bits = Table::zero_bits;
    static constexpr u64 zero_mask = (u64{1} << zero_bits) - 1;
    static constexpr u64 key_mask = ~u64{0} >> (64 - Table::key_bits);
    static constexpr u64 walk_budget = 256;
    static constexpr u32 min_leg = 64, max_leg = 1024;
    static constexpr u32 dims = Table::dims, dirs = 2 * dims;
    static constexpr coord3 site_bits = coord_bits(params_of<Table>), box = box_bits(params_of<Table>);
    // On a 1D table Langton's ant walks a grid folded into the key, x in the low half of its bits.
    static constexpr u32 ant_x_bits = dims == 1 ? Table::key_bits / 2 : site_bits[0];
    static constexpr u32 ant_y_bits = dims == 1 ? Table::key_bits - ant_x_bits : site_bits[1];

    using point = std::array<u64, 3>;
    struct entry { u64 v; const T* slot; u32 pos; };
    // path holds key << 1 | owned. Only the walker that inserted a key erases it.
    struct walker
    {
        point at = {};
        u32 leg = 0;
        bool returning = false;
        Array<u64> path;
    };

    static inline random_run* current = nullptr;

    const random_params& p;
    Random::xoshiro256starstar rng;
    u64 serial = 0;
    gtl::flat_hash_map<u64, entry> ref;
    Array<u64> keys;
    gtl::flat_hash_map<u64, u32> buckets;
    gtl::flat_hash_map<u64, u32> sides;
    bool spare = false;
    // clear() destroys each value as its scan reaches it, so a hook sees the values
    // not reached yet still present. The table stays a subset of ref.
    u32 clearing = 0;
    // value -> key of each tracked value in the table. Destructors update it, so it stays exact
    // while ref is stale during clear().
    gtl::flat_hash_map<u64, u64> where;
    u32 destroyed = 0;
    Array<u64> bases;
    u32 walk_pos = 0;
    point at = {}, home = {};
    u32 heading = 0, leg = 0;
    bool returning = false;
    Array<u8> steps;
    Array<u64> path, perimeter;
    gtl::flat_hash_map<u64, u32> per_site;
    Array<walker> walkers;
    // one bit per key: set while some walker owns the key, so the table must hold it
    Array<u64> shadow, seen;
    Table t;

    static u64 value_of(const T& x)
    {
        if constexpr (is_tracked)
            return x.v;
        else
            return u64{x};
    }

    const T* table_find(u64 k) const
    {
        if constexpr (Table::dims == 1)
            return t.find(k);
        else
        {
            const T* s = at_coords<Table>(k, [&](auto... c) { return t.find(c...); });
            fm_assert(s == t.find(k));
            return s;
        }
    }

    bool table_insert(u64 k, T value)
    {
        if constexpr (Table::dims == 1)
            return t.insert(k, move(value));
        else
            return at_coords<Table>(k, [&](auto... c) { return t.insert(c..., move(value)); });
    }

    T table_erase(u64 k)
    {
        if constexpr (Table::dims == 1)
            return t.erase(k);
        else
            return at_coords<Table>(k, [&](auto... c) { return t.erase(c...); });
    }

    static void hook(u32 action, u64 key)
    {
        random_run& r = *current;
        switch (action)
        {
        case act_erase:
            r.erase(key);
            break;
        case act_insert:
            r.insert(key, r.next_plain());
            break;
        case act_clear:
            if (!r.clearing)
                r.clear();
            break;
        }
    }

    static void on_destroy(u64 v)
    {
        random_run& r = *current;
        if (const auto it = r.where.find(v); it != r.where.end())
        {
            const u64 k = it->second;
            r.where.erase(it);
            r.check_bucket(k);
        }
        fm_assert(r.where.size() == tracked::live);
        if constexpr (free_empty)
            fm_assert(r.t.size() == tracked::live);
        const u64 cost = (u64{1} << Table::top_bits) + (u64{r.t.page_count()} << page_bits);
        if (++r.destroyed * walk_budget >= cost)
        {
            r.destroyed = 0;
            r.check_where(false);
        }
    }

    void check_bucket(u64 k)
    {
        const auto top = t.raw_top();
        if constexpr (!Table::has_pages)
            fm_assert(!top[k]);
        else if constexpr (page_bits <= 10)
        {
            const u64 ti = k >> (page_bits + zero_bits), zi = k >> page_bits & zero_mask;
            const auto& e = top[ti];
            if constexpr (zero_bits > 0)
                if (e.side)
                {
                    u32 side_refs = 0;
                    for (u32 z = 1; z <= zero_mask; z++)
                        if (detail_mlt::page_of(e.side[z]))
                            side_refs++;
                    if constexpr (free_empty)
                        fm_assert(side_refs > 0 && side_refs == e.side[0].live);
                }
            const auto* ref = [&] {
                if constexpr (zero_bits > 0)
                    return !zi ? &e.zero : e.side ? &e.side[zi] : nullptr;
                else
                    return &e;
            }();
            const T* page = ref ? detail_mlt::page_of(*ref) : nullptr;
            if (!page)
                return;
            const u64 base = k >> page_bits << page_bits;
            u32 n = 0;
            for (u32 i = 0; i < 1u << page_bits; i++)
                if (page[i])
                {
                    const auto it = where.find(value_of(page[i]));
                    fm_assert(it != where.end() && it->second == (base | i));
                    n++;
                }
            if constexpr (free_empty)
                fm_assert(n > 0 && n == ref->live);
        }
    }

    // No other tracked value is outside the table while one is destroyed.
    void check_where(bool keys)
    {
        const u64 n = check_layers(t, [&](u64 k, const T& x) {
            if (!keys)
                return;
            const auto it = where.find(value_of(x));
            fm_assert(it != where.end() && it->second == k);
        });
        fm_assert(n == where.size() && n == tracked::live);
    }

    u64 next_plain()
    {
        const u64 v = ++serial;
        if constexpr (!is_tracked && sizeof(T) < sizeof(u64))
        {
            fm_assert(v <= limits<T>::max);
            static_assert(std::is_integral_v<T>);
        }
        return v;
    }

    u64 window_key(u32 i) const
    {
        return bases[i / p.window_size] + i % p.window_size;
    }

    void make_bases()
    {
        const u32 w = p.window_size;
        fm_assert(p.windows >= 2 && w > 0);
        if constexpr (Table::has_pages)
            fm_assert(w <= 1u << (page_bits - 1));
        const u64 last = key_mask - (w - 1);
        arrayReserve(bases, p.windows);
        arrayAppend(bases, u64{0});
        arrayAppend(bases, last);
        for (u32 i = 2; i < p.windows; i++)
        {
            const u32 shift = page_bits + (i % 2 ? zero_bits : 0);
            const u64 b = i == 2 ? u64{1} << shift : (Random::next(rng) & key_mask) >> shift << shift;
            const u64 base = b < w / 2 ? 0 : b - w / 2;
            arrayAppend(bases, Math::min(base, last));
        }
    }

    u64 present_key()
    {
        return keys[below(rng, (u32)keys.size())];
    }

    u64 fresh_key()
    {
        if (p.mode == key_mode::walk)
            return window_key(below(rng, p.windows * p.window_size));
        return Random::next(rng) & key_mask;
    }

    u64 pick_key(bool ins)
    {
        if (p.mode == key_mode::walk)
        {
            const u32 n = p.windows * p.window_size;
            const u64 r = Random::next(rng);
            if (r % 64 == 0)
                walk_pos = u32((r >> 32) * n >> 32);
            else
                walk_pos = (walk_pos + n - 8 + u32(r >> 8) % 17) % n;
            return window_key(walk_pos);
        }
        if constexpr (Table::key_bits > 16)
            if (!keys.isEmpty() && Random::next(rng) % 8 < (ins ? 1u : 6u))
                return present_key();
        return fresh_key();
    }

    u64 make_value(u64 k)
    {
        if constexpr (is_tracked)
            if (p.hooks && Random::next(rng) % 8 == 0)
            {
                const u64 r = Random::next(rng);
                const u32 action = r % 64 == 0 ? act_clear : r >> 6 & 1 ? act_erase : act_insert;
                const u64 which = r >> 7 & 7;
                const u64 key = which == 0 ? k : which < 4 && !keys.isEmpty() ? present_key() : fresh_key();
                return act(action, key, serial++ & ((u64{1} << (61 - tracked::key_bits)) - 1));
            }
        return next_plain();
    }

    void ref_add(u64 k, u64 v, const T* slot)
    {
        fm_assert(slot);
        const bool added = ref.try_emplace(k, entry{v, slot, (u32)keys.size()}).second;
        fm_assert(added);
        arrayAppend(keys, k);
        if constexpr (Table::has_pages)
        {
            const auto [it, fresh] = buckets.try_emplace(k >> page_bits, 0u);
            it->second++;
            // add_page() takes the spare
            if (fresh)
                spare = false;
            if constexpr (zero_bits > 0)
                if (fresh && k >> page_bits & zero_mask)
                    sides[k >> (page_bits + zero_bits)]++;
        }
    }

    void ref_remove(u64 k)
    {
        const auto it = ref.find(k);
        fm_assert(it != ref.end());
        const u32 i = it->second.pos, last = (u32)keys.size() - 1;
        ref.erase(it);
        if (i != last)
        {
            keys[i] = keys[last];
            ref.find(keys[i])->second.pos = i;
        }
        arrayRemoveSuffix(keys, 1);
        if constexpr (Table::has_pages)
        {
            const auto b = buckets.find(k >> page_bits);
            fm_assert(b != buckets.end() && b->second > 0);
            b->second--;
            if constexpr (free_empty)
                if (!b->second)
                {
                    buckets.erase(b);
                    spare = true;
                    if constexpr (zero_bits > 0)
                        if (k >> page_bits & zero_mask)
                        {
                            const auto s = sides.find(k >> (page_bits + zero_bits));
                            fm_assert(s != sides.end() && s->second > 0);
                            if (!--s->second)
                                sides.erase(s);
                        }
                }
        }
    }

    void ref_clear()
    {
        ref.clear();
        arrayClear(keys);
        buckets.clear();
        sides.clear();
    }

    // a failed insert destroys the value after insert() returns, and its hook may change ref
    void insert(u64 k, u64 v)
    {
        const bool before = ref.contains(k);
        const bool ok = table_insert(k, T(v));
        if (clearing)
            fm_assert(ok || before);
        else
            fm_assert(ok == !before);
        if (ok)
        {
            if (before)
                ref_remove(k);
            ref_add(k, v, table_find(k));
            if constexpr (is_tracked)
            {
                const bool added = where.try_emplace(v, k).second;
                fm_assert(added);
            }
        }
    }

    void erase(u64 k)
    {
        u64 expected = 0;
        if (const auto it = ref.find(k); it != ref.end())
        {
            expected = it->second.v;
            ref_remove(k);
        }
        T got = table_erase(k);
        fm_assert(value_of(got) == expected || (clearing && !got));
    }

    void clear()
    {
        const bool had_pages = t.page_count() > 0;
        clearing++;
        t.clear();
        clearing--;
        fm_assert(t.page_count() == 0);
        if constexpr (is_tracked)
            fm_assert(where.empty());
        // clear() empties a page before freeing it only when it has destructors to run
        if constexpr (free_empty && !std::is_trivially_destructible_v<T>)
            spare = spare || had_pages;
        ref_clear();
        for (u64& x : shadow)
            x = 0;
        for (walker& w : walkers)
        {
            arrayClear(w.path);
            w.returning = false;
        }
    }

    void check_key(u64 k) const
    {
        const T* s = table_find(k);
        if (const auto it = ref.find(k); it != ref.end())
            fm_assert(s == it->second.slot && value_of(*s) == it->second.v);
        else
            fm_assert(!s || !*s);
    }

    void check_absent()
    {
        const auto absent = [&](u64 k) {
            if (!ref.contains(k))
                fm_assert(!present(t, k));
        };
        if (p.mode == key_mode::walk)
        {
            for (u32 i = 0; i < p.windows * p.window_size; i++)
                absent(window_key(i));
        }
        else if constexpr (Table::key_bits <= 18)
        {
            for (u64 k = 0; k <= key_mask; k++)
                absent(k);
        }
        else
        {
            for (u64 k : keys)
            {
                if (k > 0)
                    absent(k - 1);
                if (k < key_mask)
                    absent(k + 1);
            }
            for (u32 i = 0; i < 256; i++)
                absent(Random::next(rng) & key_mask);
        }
    }

    void check()
    {
        const auto top = t.raw_top();
        u64 n = 0;
        const auto visit = [&](u64 k, const T& x) {
            const auto it = ref.find(k);
            fm_assert(it != ref.end() && it->second.v == value_of(x) && it->second.slot == &x);
            n++;
        };
        if constexpr (!Table::has_pages)
        {
            for (u32 k = 0; k < top.size(); k++)
                if (top[k])
                    visit(k, top[k]);
        }
        else
        {
            fm_assert(t.page_count() == buckets.size());
            for (const auto& rec : t.raw_pages())
            {
                const u64 bucket = u64{rec.top_index} << zero_bits | rec.zero_index;
                u32 live = 0;
                for (u32 i = 0; i < 1u << page_bits; i++)
                    if (rec.page[i])
                    {
                        visit(bucket << page_bits | i, rec.page[i]);
                        live++;
                    }
                const auto b = buckets.find(bucket);
                fm_assert(b != buckets.end() && b->second == live);
            }
            if constexpr (zero_bits > 0)
            {
                u32 num_sides = 0;
                for (const auto& [i, slot] : enumerate(top))
                    if (slot.side)
                    {
                        const auto s = sides.find(i);
                        fm_assert(s != sides.end());
                        if constexpr (free_empty)
                            fm_assert(slot.side[0].live == s->second);
                        num_sides++;
                    }
                fm_assert(num_sides == sides.size());
            }
            fm_assert(!!t.raw_spare().page == spare);
        }
        fm_assert(n == ref.size());
        for (const auto& [k, e] : ref)
        {
            const T* s = t.find(k);
            fm_assert(s == e.slot && value_of(*s) == e.v);
        }
        if constexpr (free_empty)
            fm_assert(t.size() == n);
        if constexpr (is_tracked)
        {
            fm_assert(tracked::live == n && where.size() == n);
            check_where(true);
        }
        check_absent();
        check_layout(t, p.top_size_bytes, keys);
        if (p.walk == walk_type::retrace)
            check_owners();
    }

    bool phase_insert(u32 op)
    {
        const u32 insert_fifths = op / 2000 % 2 ? 1 : 4;
        return Random::next(rng) % 5 < insert_fifths && keys.size() < p.live_cap;
    }

    void touch(u64 k, bool ins)
    {
        if (ins)
            insert(k, make_value(k));
        else
            erase(k);
        check_key(k);
    }

    void drain()
    {
        while (!keys.isEmpty())
        {
            const u64 k = present_key();
            erase(k);
            check_key(k);
        }
        if constexpr (is_tracked)
            fm_assert(tracked::live == 0);
        if constexpr (free_empty)
        {
            fm_assert(t.page_count() == 0 && t.size() == 0);
            if constexpr (zero_bits > 0)
                for (const auto& e : t.raw_top())
                    fm_assert(!e.side);
        }
    }

    u64 key_at(const point& s) const
    {
        if constexpr (dims == 1)
            return s[0];
        else
            return test_pack<params_of<Table>>({u32(s[0]), u32(s[1]), u32(s[2])});
    }

    point point_of(u64 k) const
    {
        if constexpr (dims == 1)
            return {k, 0, 0};
        else
        {
            const coord3 c = test_unpack<params_of<Table>>(k);
            return {c[0], c[1], c[2]};
        }
    }

    static point moved(point s, u32 dir, u64 n = 1)
    {
        const u32 d = dir / 2;
        s[d] = (dir & 1 ? s[d] - n : s[d] + n) & low_mask(site_bits[d]);
        return s;
    }

    static u64 torus_distance(const point& a, const point& b)
    {
        u64 n = 0;
        for (u32 d = 0; d < dims; d++)
        {
            const u64 m = low_mask(site_bits[d]), x = (a[d] - b[d]) & m, y = (b[d] - a[d]) & m;
            n += Math::min(x, y);
        }
        return n;
    }

    point random_point()
    {
        point s = {};
        for (u32 d = 0; d < dims; d++)
            s[d] = Random::next(rng) & low_mask(site_bits[d]);
        return s;
    }

    point page_corner()
    {
        point s = random_point();
        for (u32 d = 0; d < dims; d++)
            s[d] &= ~low_mask(box[d]);
        return s;
    }

    u32 pick_weighted(const u32 (&w)[6])
    {
        u32 sum = 0, i = 0;
        for (u32 dir = 0; dir < dirs; dir++)
            sum += w[dir];
        for (u32 r = below(rng, sum); r >= w[i]; i++)
            r -= w[i];
        return i;
    }

    u32 visits(u64 k) const
    {
        const auto it = per_site.find(k);
        return it != per_site.end() ? it->second : 0;
    }

    u32 grow_cap() const
    {
        if constexpr (dims == 1)
            return 24;
        else
            return key_mask / 4 < 256 ? u32(key_mask / 4 + 1) : 256;
    }

    void walk_begin()
    {
        at = random_point();
        if (p.walk == walk_type::vertex_reinforced)
            at = page_corner();
        else if (p.walk == walk_type::langton_ant)
            at = { Random::next(rng) & low_mask(ant_x_bits), Random::next(rng) & low_mask(ant_y_bits), at[2] };
        else if (p.walk == walk_type::retrace)
        {
            fm_assert(Table::key_bits <= 20 && p.walkers > 0);
            shadow = Array<u64>{ValueInit, usize(key_mask >> 6) + 1};
            seen = Array<u64>{ValueInit, usize(key_mask >> 6) + 1};
            walkers = Array<walker>{ValueInit, p.walkers};
            for (walker& w : walkers)
            {
                w.at = random_point();
                arrayReserve(w.path, max_leg + 1);
            }
        }
        else if (p.walk == walk_type::elephant)
            arrayReserve(steps, Math::max(p.ops, 16u));
        else if (p.walk == walk_type::loop_erased)
            arrayReserve(path, 16);
        else if (p.walk == walk_type::eden)
            arrayReserve(perimeter, 16);
        home = at;
    }

    void kinetic_growth_step()
    {
        if (!ref.contains(key_at(at)))
        {
            touch(key_at(at), true);
            return;
        }
        u32 open[6], n = 0;
        for (u32 dir = 0; dir < dirs; dir++)
            if (!ref.contains(key_at(moved(at, dir))))
                open[n++] = dir;
        if (!n)
        {
            drain();
            at = random_point();
            return;
        }
        at = moved(at, open[below(rng, n)]);
        touch(key_at(at), true);
    }

    void loop_erased_step()
    {
        if (path.isEmpty())
        {
            per_site[key_at(at)] = 0;
            arrayAppend(path, key_at(at));
            touch(key_at(at), true);
        }
        at = moved(at, below(rng, dirs));
        const u64 k = key_at(at);
        if (const auto it = per_site.find(k); it != per_site.end())
        {
            const u32 keep = it->second + 1;
            while (path.size() > keep)
            {
                const u64 j = path.back();
                arrayRemoveSuffix(path, 1);
                per_site.erase(j);
                touch(j, false);
            }
        }
        else
        {
            per_site[k] = (u32)path.size();
            arrayAppend(path, k);
            touch(k, true);
        }
    }

    void langton_ant_step()
    {
        const u64 k = dims == 1 ? at[0] | at[1] << ant_x_bits : key_at(at);
        const bool black = ref.contains(k);
        heading = (heading + (black ? 3 : 1)) % 4;
        touch(k, !black);
        const u32 d = heading % 2;
        at[d] = (heading & 2 ? at[d] - 1 : at[d] + 1) & low_mask(d ? ant_y_bits : ant_x_bits);
    }

    void internal_dla_step()
    {
        if (keys.size() >= grow_cap())
        {
            drain();
            home = random_point();
        }
        at = home;
        while (ref.contains(key_at(at)))
            at = moved(at, below(rng, dirs));
        touch(key_at(at), true);
    }

    void eden_step()
    {
        if (keys.size() >= grow_cap())
        {
            drain();
            home = random_point();
        }
        if (keys.isEmpty() || perimeter.isEmpty())
        {
            arrayClear(perimeter);
            per_site.clear();
            at = home;
        }
        else
        {
            const u32 i = below(rng, (u32)perimeter.size());
            const u64 k = perimeter[i];
            per_site.erase(k);
            if (i + 1 != perimeter.size())
            {
                perimeter[i] = perimeter.back();
                per_site[perimeter[i]] = i;
            }
            arrayRemoveSuffix(perimeter, 1);
            at = point_of(k);
        }
        if (!ref.contains(key_at(at)))
            touch(key_at(at), true);
        for (u32 dir = 0; dir < dirs; dir++)
        {
            const u64 k = key_at(moved(at, dir));
            if (!ref.contains(k) && per_site.try_emplace(k, (u32)perimeter.size()).second)
                arrayAppend(perimeter, k);
        }
    }

    static bool bit(const Array<u64>& a, u64 k)
    {
        return (a[k >> 6] >> (k & 63) & 1) != 0;
    }

    static void flip(Array<u64>& a, u64 k)
    {
        a[k >> 6] ^= u64{1} << (k & 63);
    }

    void retrace_visit(walker& w)
    {
        const u64 k = key_at(w.at);
        const bool own = !bit(shadow, k);
        fm_assert(own == !ref.contains(k));
        if (own)
        {
            flip(shadow, k);
            touch(k, true);
        }
        else
            check_key(k);
        arrayAppend(w.path, k << 1 | u64{own});
    }

    void retrace_back(walker& w)
    {
        const u64 e = w.path.back(), k = e >> 1;
        arrayRemoveSuffix(w.path, 1);
        if (e & 1)
        {
            fm_assert(bit(shadow, k));
            flip(shadow, k);
            touch(k, false);
        }
        else
            check_key(k);
        if (!w.path.isEmpty())
            w.at = point_of(w.path.back() >> 1);
        else
            w.returning = false;
    }

    void retrace_step()
    {
        walker& w = walkers[below(rng, (u32)walkers.size())];
        if (w.returning)
        {
            retrace_back(w);
            // a lone walker's exact reversal leaves nothing behind
            if (walkers.size() == 1 && w.path.isEmpty())
            {
                fm_assert(keys.isEmpty());
                drain();
            }
            return;
        }
        if (w.path.isEmpty())
        {
            w.leg = min_leg + below(rng, max_leg - min_leg + 1);
            if (walkers.size() > 1 && below(rng, 4) == 0)
                w.at = walkers[below(rng, (u32)walkers.size())].at;
            retrace_visit(w);
        }
        w.at = moved(w.at, below(rng, dirs));
        retrace_visit(w);
        w.returning = w.path.size() > w.leg;
    }

    void retrace_finish()
    {
        Array<u32> busy;
        arrayReserve(busy, walkers.size());
        for (const auto& [i, w] : enumerate(walkers))
            if (!w.path.isEmpty())
                arrayAppend(busy, i);
        while (!busy.isEmpty())
        {
            const u32 i = below(rng, (u32)busy.size());
            walker& w = walkers[busy[i]];
            retrace_back(w);
            if (w.path.isEmpty())
            {
                busy[i] = busy.back();
                arrayRemoveSuffix(busy, 1);
            }
        }
        fm_assert(keys.isEmpty());
        for (u64 x : shadow)
            fm_assert(!x);
        drain();
    }

    void check_owners()
    {
        for (u64 k = 0; k <= key_mask; k++)
            fm_assert(bit(shadow, k) == ref.contains(k));
        for (u64& x : seen)
            x = 0;
        u64 owned = 0;
        for (const walker& w : walkers)
            for (u64 e : w.path)
                if (e & 1)
                {
                    const u64 k = e >> 1;
                    fm_assert(bit(shadow, k) && !bit(seen, k));
                    flip(seen, k);
                    owned++;
                }
        fm_assert(owned == keys.size());
    }

    void walk_step(u32 op)
    {
        switch (p.walk)
        {
        case walk_type::none:
            fm_abort("no walk");
        case walk_type::simple:
            at = moved(at, below(rng, dirs));
            break;
        case walk_type::levy_flight: {
            // Pr(length >= l) = l^-1.5
            const u32 dir = below(rng, dirs);
            const double u = double((Random::next(rng) >> 11) + 1) * 0x1p-53;
            at = moved(at, dir, u64(std::pow(u, -1 / 1.5)));
            break;
        }
        case walk_type::persistent:
            if (op == 1 || below(rng, 20) == 0)
                heading = below(rng, dirs);
            at = moved(at, heading);
            break;
        case walk_type::kinetic_growth:
            kinetic_growth_step();
            return;
        case walk_type::true_self_avoiding: {
            u32 w[6] = {};
            for (u32 dir = 0; dir < dirs; dir++)
            {
                const u32 n = visits(key_at(moved(at, dir)));
                w[dir] = 1u << (20 - Math::min(n, 20u));
            }
            at = moved(at, pick_weighted(w));
            per_site[key_at(at)]++;
            break;
        }
        case walk_type::loop_erased:
            loop_erased_step();
            return;
        case walk_type::vertex_reinforced: {
            u32 w[6] = {};
            for (u32 dir = 0; dir < dirs; dir++)
                w[dir] = 1 + visits(key_at(moved(at, dir)));
            at = moved(at, pick_weighted(w));
            per_site[key_at(at)]++;
            touch(key_at(at), !ref.contains(key_at(at)));
            return;
        }
        case walk_type::elephant: {
            u32 dir = below(rng, dirs);
            if (!steps.isEmpty())
            {
                const u32 past = steps[below(rng, (u32)steps.size())];
                const u32 other = below(rng, dirs - 1);
                dir = below(rng, 10) < 9 ? past : other + (other >= past);
            }
            arrayAppend(steps, u8(dir));
            at = moved(at, dir);
            break;
        }
        case walk_type::resetting: {
            if (below(rng, 64) == 0)
                at = home;
            else
                at = moved(at, below(rng, dirs));
            const u32 insert_fifths = torus_distance(at, home) <= 3 ? 4 : 1;
            touch(key_at(at), Random::next(rng) % 5 < insert_fifths && keys.size() < p.live_cap);
            return;
        }
        case walk_type::random_environment: {
            // each site's bias is 4:1 up or down per dimension, with E[log ratio] = 0
            const u32 d = below(rng, dims);
            const bool up = (mix64(key_at(at) ^ p.seed) >> d & 1) != 0;
            const bool plus = below(rng, 5) < (up ? 4u : 1u);
            at = moved(at, d * 2 + !plus);
            break;
        }
        case walk_type::excited: {
            const bool first = per_site.try_emplace(key_at(at), 1u).second;
            u32 dir = below(rng, dirs);
            if (first && dir == 1 && (Random::next(rng) & 1))
                dir = 0;
            at = moved(at, dir);
            break;
        }
        case walk_type::langton_ant:
            langton_ant_step();
            return;
        case walk_type::rotor_router: {
            const u64 k = key_at(at);
            const auto it = per_site.try_emplace(k, u32(mix64(k ^ p.seed) % dirs)).first;
            it->second = (it->second + 1) % dirs;
            at = moved(at, it->second);
            break;
        }
        case walk_type::internal_dla:
            internal_dla_step();
            return;
        case walk_type::eden:
            eden_step();
            return;
        case walk_type::retrace:
            retrace_step();
            return;
        case walk_type::page_corner_drift: {
            const u32 d = below(rng, dims);
            const u64 m = low_mask(box[d]), o = at[d] & m;
            bool minus = (Random::next(rng) & 1) != 0;
            if (o != 0 && o != m && below(rng, 4) != 0)
                minus = o <= m - o;
            at = moved(at, d * 2 + minus);
            break;
        }
        }
        touch(key_at(at), phase_insert(op));
    }

public:
    explicit random_run(const random_params& p) : p{p}
    {
        Random::seed(rng, p.seed);
        arrayReserve(keys, 16);
    }

    void run()
    {
        current = this;
        tracked::hook = hook;
        tracked::on_destroy = on_destroy;
        if (p.mode == key_mode::walk)
            make_bases();
        if (p.walk != walk_type::none)
            walk_begin();
        for (u32 op = 1; op <= p.ops; op++)
        {
            if (p.walk != walk_type::none)
                walk_step(op);
            else
            {
                const bool ins = phase_insert(op);
                const u64 k = pick_key(ins);
                if (ins)
                    insert(k, make_value(k));
                else
                    erase(k);
                check_key(k);
            }
            if (t.page_count() >= p.page_cap || op % p.clear_every == 0)
                clear();
            else if (op % p.move_every == 0)
            {
                Table tmp{move(t)};
                t = move(tmp);
            }
            if (op % p.check_every == 0)
                check();
        }
        if (p.walk == walk_type::retrace)
            retrace_finish();
        check();
        clear();
        check();
        current = nullptr;
        tracked::hook = nullptr;
        tracked::on_destroy = nullptr;
    }
};

template<typename Table>
void check_random(const random_params& p)
{
    static_assert(top_bytes<Table> == (usize)(u32)top_bytes<Table>);
    random_run<Table>{p}.run();
}

template<typename Table>
void check_walk(u64 seed, walk_type walk, u32 ops = 3000)
{
    check_random<Table>({.seed = seed, .ops = ops, .top_size_bytes = top_bytes<Table>, .walk = walk,
                         .hooks = walk != walk_type::retrace, .check_every = 500, .move_every = 1000});
}

// each walker makes about two round trips
template<typename Table>
void check_retrace(u64 seed, u32 walkers)
{
    check_random<Table>({.seed = seed, .ops = walkers * 2200, .top_size_bytes = top_bytes<Table>,
                         .walk = walk_type::retrace, .walkers = walkers, .check_every = 500, .move_every = 1000});
}

constexpr random_params with(u64 seed, u32 top_size_bytes)
{
    return { .seed = seed, .ops = 8000, .top_size_bytes = top_size_bytes, .hooks = true,
             .check_every = 512, .clear_every = 2000, .move_every = 1000 };
}

template<typename Table>
void check_fill_block(u64 block, Random::xoshiro256starstar& rng)
{
    using T = std::remove_cvref_t<decltype(*std::declval<const Table&>().find(0))>;
    constexpr bool free_empty = requires (const Table& t) { t.size(); };
    constexpr u32 bits = Table::has_pages ? Table::page_bits : Table::key_bits;
    constexpr u32 n = 1u << bits, zero_bits = Table::zero_bits;
    constexpr u64 last_block = (u64{1} << (Table::key_bits - bits)) - 1;
    const u64 base = block << bits;
    const auto value = [](u64 k) {
        if constexpr (std::is_same_v<T, tracked>)
            return tracked{k + 1};
        else
            return T(k % 100 + 1);
    };

    Table t;
    [[maybe_unused]] constexpr coord3 box = box_bits(params_of<Table>);
    [[maybe_unused]] const coord3 origin = [&] {
        if constexpr (Table::dims > 1)
            return test_unpack<params_of<Table>>(base);
        else
            return coord3{};
    }();
    const auto key_of = [&](u32 i) -> u64 {
        if constexpr (Table::dims == 1)
            return base | i;
        else
        {
            coord3 c = origin;
            for (u32 d = 0, shift = 0; d < Table::dims; shift += box[d], d++)
                c[d] += i >> shift & ((1u << box[d]) - 1);
            return test_pack<params_of<Table>>(c);
        }
    };
    const auto insert = [&](u64 k) {
        if constexpr (Table::dims == 1)
            return t.insert(k, value(k));
        else
            return at_coords<Table>(k, [&](auto... c) { return t.insert(c..., value(k)); });
    };
    const auto erase = [&](u64 k) {
        if constexpr (Table::dims == 1)
            return t.erase(k);
        else
            return at_coords<Table>(k, [&](auto... c) { return t.erase(c...); });
    };

    for (u32 i = 0; i < n; i++)
        fm_assert(insert(key_of(i)));
    const u64 found = check_layers(t, [&](u64 k, const T&) { fm_assert(k >> bits == block); });
    fm_assert(found == n);
    if (block > 0)
        fm_assert(!present(t, base - 1) && !present(t, base - n));
    if (block < last_block)
        fm_assert(!present(t, base + n));
    if constexpr (Table::dims > 1)
    {
        constexpr coord3 dim_bits = coord_bits(params_of<Table>);
        const auto absent = [&](coord3 c) {
            const T* s = at_coords<Table>(test_pack<params_of<Table>>(c), [&](auto... xs) { return t.find(xs...); });
            fm_assert(!s || !*s);
        };
        for (u32 d = 0; d < Table::dims; d++)
        {
            coord3 c = origin;
            if (origin[d] > 0)
            {
                c[d] = origin[d] - 1;
                absent(c);
            }
            if ((u64{origin[d]} + (u64{1} << box[d])) >> dim_bits[d] == 0)
            {
                c[d] = origin[d] + (1u << box[d]);
                absent(c);
            }
        }
    }

    [[maybe_unused]] const T* page = nullptr;
    [[maybe_unused]] u32 ti = 0, zi = 0;
    if constexpr (Table::has_pages)
    {
        fm_assert(t.page_count() == 1);
        const auto rec = t.raw_pages()[0];
        page = rec.page;
        ti = rec.top_index;
        zi = rec.zero_index;
        fm_assert((u64{ti} << zero_bits | zi) == block);
        const auto top = t.raw_top();
        if constexpr (zero_bits > 0)
            for (u32 i = 0; i < top.size(); i++)
                fm_assert(!top[i].side == (i != ti || !zi));
        if constexpr (free_empty)
            fm_assert(ref_at<zero_bits>(top, ti, zi).live == n && t.size() == n);
    }

    Array<u32> order{NoInit, n};
    for (u32 i = 0; i < n; i++)
    {
        const u32 j = below(rng, i + 1);
        if (j != i)
            order[i] = order[j];
        order[j] = i;
    }
    for (u32 i = 0; i < n; i++)
    {
        const T got = erase(key_of(order[i]));
        fm_assert(got);
        if constexpr (free_empty)
        {
            if (i + 1 < n)
                fm_assert(ref_at<zero_bits>(t.raw_top(), ti, zi).live == n - i - 1);
        }
    }
    fm_assert(check_layers(t, [](u64, const T&) {}) == 0);
    if constexpr (Table::has_pages)
    {
        if constexpr (free_empty)
        {
            fm_assert(t.page_count() == 0 && t.raw_spare().page == page);
            if constexpr (zero_bits > 0)
                for (const auto& e : t.raw_top())
                    fm_assert(!e.side);
        }
        else
            fm_assert(t.page_count() == 1 && t.raw_pages()[0].page == page);
    }
}

template<typename Table>
void check_fill(u64 seed)
{
    constexpr u32 bits = Table::has_pages ? Table::page_bits : Table::key_bits;
    constexpr u64 last_block = (u64{1} << (Table::key_bits - bits)) - 1;
    Random::xoshiro256starstar rng;
    Random::seed(rng, seed);
    check_fill_block<Table>(0, rng);
    if constexpr (last_block > 0)
    {
        check_fill_block<Table>(last_block, rng);
        check_fill_block<Table>(Random::next(rng) & last_block, rng);
    }
}

template<typename Table>
void check_pack(u64 seed)
{
    constexpr coord3 box = box_bits(params_of<Table>), dim_bits = coord_bits(params_of<Table>);
    constexpr u32 block_bits = Table::has_pages ? Table::page_bits : Table::key_bits;
    constexpr u64 block_mask = (u64{1} << block_bits) - 1;
    constexpr u64 key_mask = ~u64{0} >> (64 - Table::key_bits);
    gtl::flat_hash_map<u64, coord3> block_high;
    const auto check_key = [&](u64 k) {
        const coord3 c = test_unpack<params_of<Table>>(k);
        coord3 high = {};
        u64 slot = 0;
        for (u32 d = 0, shift = 0; d < 3; shift += box[d], d++)
        {
            fm_assert(!(u64{c[d]} >> dim_bits[d]));
            slot |= (u64{c[d]} & ((u64{1} << box[d]) - 1)) << shift;
            high[d] = u32(u64{c[d]} >> box[d]);
        }
        fm_assert(slot == (k & block_mask));
        fm_assert(block_high.try_emplace(k >> block_bits, high).first->second == high);
        at_coords<Table>(k, [](auto...) {});
    };
    if constexpr (Table::key_bits <= 16)
    {
        for (u64 k = 0; k <= key_mask; k++)
            check_key(k);
    }
    else
    {
        Random::xoshiro256starstar rng;
        Random::seed(rng, seed);
        for (u32 i = 0; i < 4096; i++)
            check_key(Random::next(rng) & key_mask);
        for (u32 m = 0; m < 1u << Table::dims; m++)
        {
            coord3 c = {};
            for (u32 d = 0; d < Table::dims; d++)
                if (m >> d & 1)
                    c[d] = u32((u64{1} << dim_bits[d]) - 1);
            check_key(test_pack<params_of<Table>>(c));
        }
    }
}

} // namespace floormat::mlt_test
