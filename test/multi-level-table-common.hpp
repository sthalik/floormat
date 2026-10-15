#pragma once
#include "compat/defs.hpp"
#include "compat/multi-level-table.inl"
#include "random/random.hpp"
#include <array>
#include <cmath>
#include <gtl/phmap.hpp>
#include <limits>
#include <type_traits>
#include <utility>
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace floormat {

template<mlt_params P>
struct mlt_coord_traits<std::array<uint32_t, 3>, P>
{
    static constexpr mlt_coords coords(const std::array<uint32_t, 3>& c) noexcept { return {{ c[0], c[1], c[2] }}; }
};

} // namespace floormat

namespace floormat::mlt_test {

struct tracked
{
    static constexpr uint64_t act_flag = uint64_t{1} << 63;
    static constexpr uint32_t key_bits = 40;
    static uint32_t live;
    static void (*hook)(uint32_t action, uint64_t key);
    static void (*on_destroy)(uint64_t v);

    uint64_t v = 0;

    tracked() noexcept = default;
    explicit tracked(uint64_t v) noexcept : v{v} { live++; }
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
            hook(uint32_t(x >> 61) & 3, x & ((uint64_t{1} << key_bits) - 1));
    }
};

void test_bit_tables();
void test_plane_tables();
void test_space_tables();

enum : uint32_t { act_erase, act_insert, act_clear, };

static_assert(!std::is_copy_constructible_v<tracked> && !std::is_copy_assignable_v<tracked>);

// A value whose destructor calls tracked::hook. Bits 61..62 hold the action, 0..39 the key.
// The tag in between keeps values unique.
constexpr uint64_t act(uint32_t action, uint64_t key, uint64_t tag = 0)
{
    fm_assert(!(key >> tracked::key_bits) && !(tag >> (61 - tracked::key_bits)));
    return tracked::act_flag | uint64_t{action} << 61 | tag << tracked::key_bits | key;
}

template<typename Table> Table* target = nullptr;

template<typename Table>
void act_on(uint32_t action, uint64_t key)
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

using coord3 = std::array<uint32_t, 3>;

template<typename Table> constexpr inline mlt_params params_of = {};
template<typename T, mlt_params P> constexpr inline mlt_params params_of<multi_level_table<T, P>> = P;

// Key bit -> the coordinate and coordinate bit it holds. Built from the top of the key down,
// apart from the table's pack(), which builds from the bottom up.
template<mlt_params P>
struct key_layout
{
    uint8_t dim[64] = {}, bit[64] = {};

    consteval key_layout()
    {
        uint32_t below[3] = { P.dim_bits(0), P.dim_bits(1), P.dim_bits(2) };
        uint32_t b = P.key_bits();
        for (uint32_t i = 0; i < P.depth(); i++)
        {
            const mlt_bits& level = P.levels[i].bits;
            for (uint32_t d = level.dims; d-- > 0; )
            {
                below[d] -= level.dim[d];
                for (uint32_t j = level.dim[d]; j-- > 0; )
                {
                    b--;
                    dim[b] = uint8_t(d);
                    bit[b] = uint8_t(below[d] + j);
                }
            }
        }
        fm_assert(b == 0);
    }
};

template<mlt_params P> constexpr inline key_layout<P> layout_of{};

template<mlt_params P>
constexpr uint64_t test_pack(coord3 c)
{
    constexpr uint32_t key_bits = P.key_bits();
    uint64_t k = 0;
    for (uint32_t b = 0; b < key_bits; b++)
        k |= uint64_t{c[layout_of<P>.dim[b]] >> layout_of<P>.bit[b] & 1} << b;
    return k;
}

template<mlt_params P>
constexpr coord3 test_unpack(uint64_t k)
{
    constexpr uint32_t key_bits = P.key_bits();
    coord3 c = {};
    for (uint32_t b = 0; b < key_bits; b++)
        c[layout_of<P>.dim[b]] |= uint32_t(k >> b & 1) << layout_of<P>.bit[b];
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
decltype(auto) at_coords(uint64_t k, F&& f)
{
    const coord3 c = test_unpack<params_of<Table>>(k);
    fm_assert(test_pack<params_of<Table>>(c) == k);
    fm_assert(Table::pack(c) == k);
    return f(c);
}

template<typename Table>
constexpr bool packs_to(coord3 c, uint64_t k)
{
    return Table::pack(c) == k && test_pack<params_of<Table>>(c) == k && test_unpack<params_of<Table>>(k) == c;
}

using chunk_like_table = multi_level_table<uint64_t, chunk_like_params>;
using id_like_table = multi_level_table<tracked, id_like_params>;
using split_table = multi_level_table<uint32_t, split_params>;
using flat_table = multi_level_table<uint16_t, flat_params>;
using deep_table = multi_level_table<uint64_t, deep_params>;
using deep_tracked_table = multi_level_table<tracked, deep_params>;
using no_pages_table = multi_level_table<tracked, no_pages_params>;
using one_split_table = multi_level_table<tracked, one_split_params>;
using one_entry_table = multi_level_table<uint64_t, one_entry_params>;
using wide_table = multi_level_table<uint8_t, wide_params>;
using bit_page_table = multi_level_table<tracked, bit_page_params>;
using bit_flat_table = multi_level_table<tracked, bit_flat_params>;
using bit_flat_int_table = multi_level_table<uint32_t, bit_flat_params>;
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
using plane_wide_table = multi_level_table<uint8_t, plane_wide_params>;
using space_free_table = multi_level_table<tracked, space_free_params>;
using space_deep_table = multi_level_table<tracked, space_deep_params>;
using space_deep_int_table = multi_level_table<uint32_t, space_deep_params>;
using space_zero_free_table = multi_level_table<tracked, space_zero_free_params>;
using space_no_pages_table = multi_level_table<tracked, space_no_pages_params>;
using space_one_entry_table = multi_level_table<tracked, space_one_entry_params>;
using plane_large_table = multi_level_table<tracked, plane_large_params>;
using space_large_table = multi_level_table<tracked, space_large_params>;

template<typename Table>
constexpr bool move_only = !std::is_copy_constructible_v<Table> && !std::is_copy_assignable_v<Table> &&
                           std::is_nothrow_move_constructible_v<Table> && std::is_nothrow_move_assignable_v<Table>;

template<typename Table>
constexpr size_t top_bytes = sizeof(typename decltype(std::declval<const Table&>().raw_top())::Type) << Table::top_bits;

template<typename Table>
bool present(const Table& t, uint64_t key)
{
    const auto* s = t.find(key);
    return s && *s;
}

template<uint32_t zero_bits, typename Top>
const auto& ref_at(const Top& top, uint64_t ti, uint64_t zi)
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
uint64_t check_layers(const multi_level_table<T, P>& t, Visit&& visit)
{
    using Table = multi_level_table<T, P>;
    const auto top = t.raw_top();
    uint64_t total = 0;

    if constexpr (!Table::has_pages)
    {
        for (uint32_t k = 0; k < top.size(); k++)
            if (detail_mlt::occupied(top[k]))
            {
                visit(uint64_t{k}, top[k]);
                total++;
            }
    }
    else
    {
        constexpr uint32_t page_bits = Table::page_bits, zero_bits = Table::zero_bits;
        constexpr uint32_t page_size = 1u << page_bits, zero_mask = (1u << zero_bits) - 1;

        const auto pages = t.raw_pages();
        for (uint32_t i = 0; i < pages.size(); i++)
        {
            const auto& rec = pages[i];
            fm_assert(rec.page && rec.page != t.raw_spare().page);
            fm_assert(detail_mlt::page_of(ref_at<zero_bits>(top, rec.top_index, rec.zero_index)) == rec.page);
            for (uint32_t j = 0; j < i; j++)
                fm_assert(pages[j].page != rec.page);
            const uint64_t base = (uint64_t{rec.top_index} << zero_bits | rec.zero_index) << page_bits;
            uint32_t n = 0;
            for (uint32_t k = 0; k < page_size; k++)
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
        uint32_t num_refs = 0;
        for (const auto& e : top)
        {
            if constexpr (zero_bits > 0)
            {
                if (detail_mlt::page_of(e.zero))
                    num_refs++;
                if (e.side)
                {
                    fm_assert(!detail_mlt::page_of(e.side[0]));
                    uint32_t side_refs = 0;
                    for (uint32_t z = 1; z <= zero_mask; z++)
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
            for (uint32_t k = 0; k < page_size; k++)
                fm_assert(!detail_mlt::occupied(spare[k]));
    }
    return total;
}

#if fm_ASAN
template<typename U>
void check_heap_array(const U* p, uint32_t n)
{
    const size_t bytes = size_t{n} * sizeof(U);
    fm_assert(!__asan_region_is_poisoned(const_cast<U*>(p), bytes));
    fm_assert(__asan_address_is_poisoned(reinterpret_cast<const char*>(p) + bytes));
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
void check_layout(const multi_level_table<T, P>& t, size_t top_size_bytes, ArrayView<const uint64_t> keys)
{
    using Table = multi_level_table<T, P>;
    const auto top = t.raw_top();
    fm_assert(top.size() == 1u << Table::top_bits);
    fm_assert(sizeof(top[0]) << Table::top_bits == top_size_bytes);
    if constexpr (P.top_source == mlt_source::superpage)
        fm_assert(t.raw_top_alloc().ptr == top.data() && t.raw_top_alloc().size == top_size_bytes);
    else
        fm_assert(!t.raw_top_alloc().ptr);

    (void)check_layers(t, [](uint64_t, const T&) {});
#if fm_ASAN
    check_heap_layers(t);
#endif

    if constexpr (!Table::has_pages)
    {
        for (uint64_t k : keys)
            fm_assert(t.find(k) == &top[k] && top[k]);
    }
    else
    {
        constexpr uint32_t page_bits = Table::page_bits, zero_bits = Table::zero_bits;
        constexpr uint64_t page_mask = (uint64_t{1} << page_bits) - 1, zero_mask = (uint64_t{1} << zero_bits) - 1;
        for (uint64_t k : keys)
        {
            const T* s = t.find(k);
            fm_assert(s && *s);
            const T* page = detail_mlt::page_of(ref_at<zero_bits>(top, k >> (page_bits + zero_bits), (k >> page_bits) & zero_mask));
            fm_assert(s == page + (k & page_mask));
        }
    }
}

enum class key_mode : uint8_t { uniform, walk, };

enum class walk_type : uint8_t
{
    none, simple, levy_flight, persistent, kinetic_growth, true_self_avoiding, loop_erased, vertex_reinforced,
    elephant, resetting, random_environment, excited, langton_ant, rotor_router, internal_dla, eden, retrace,
    page_corner_drift,
};

struct random_params
{
    uint64_t seed = 0;
    uint32_t ops = 0;
    size_t top_size_bytes = 0;
    key_mode mode = key_mode::uniform;
    walk_type walk = walk_type::none;
    uint32_t walkers = 1;
    uint32_t windows = 0, window_size = 0;
    bool hooks = false;
    uint32_t check_every = 1024, clear_every = 20000, move_every = 5000;
    uint32_t live_cap = (uint32_t)-1, page_cap = (uint32_t)-1;
};

inline uint32_t below(Random::xoshiro256starstar& rng, uint32_t n)
{
    return uint32_t((Random::next(rng) >> 32) * n >> 32);
}

constexpr uint64_t low_mask(uint32_t bits)
{
    return bits ? ~uint64_t{0} >> (64 - bits) : 0;
}

constexpr uint64_t mix64(uint64_t x)
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
    static constexpr uint32_t page_bits = Table::page_bits, zero_bits = Table::zero_bits;
    static constexpr uint64_t zero_mask = (uint64_t{1} << zero_bits) - 1;
    static constexpr uint64_t key_mask = ~uint64_t{0} >> (64 - Table::key_bits);
    static constexpr uint64_t walk_budget = 256;
    static constexpr uint32_t dims = Table::dims, dirs = 2 * dims;
    static constexpr coord3 site_bits = coord_bits(params_of<Table>), box = box_bits(params_of<Table>);
    // On a 1D table Langton's ant walks a grid folded into the key, x in the low half of its bits.
    static constexpr uint32_t ant_x_bits = dims == 1 ? Table::key_bits / 2 : site_bits[0];
    static constexpr uint32_t ant_y_bits = dims == 1 ? Table::key_bits - ant_x_bits : site_bits[1];

    using point = std::array<uint64_t, 3>;
    struct entry { uint64_t v; const T* slot; uint32_t pos; };
    // path holds key << 1 | owned. Only the walker that inserted a key erases it.
    struct walker
    {
        point at = {};
        uint32_t leg = 0;
        bool returning = false;
        Array<uint64_t> path;
    };

    static inline random_run* current = nullptr;

    const random_params& p;
    Random::xoshiro256starstar rng;
    uint64_t serial = 0;
    gtl::flat_hash_map<uint64_t, entry> ref;
    Array<uint64_t> keys;
    gtl::flat_hash_map<uint64_t, uint32_t> buckets;
    gtl::flat_hash_map<uint64_t, uint32_t> sides;
    bool spare = false;
    // clear() destroys each value as its scan reaches it, so a hook sees the values
    // not reached yet still present. The table stays a subset of ref.
    uint32_t clearing = 0;
    // value -> key of each tracked value in the table. Destructors update it, so it stays exact
    // while ref is stale during clear().
    gtl::flat_hash_map<uint64_t, uint64_t> where;
    uint32_t destroyed = 0;
    Array<uint64_t> bases;
    uint32_t walk_pos = 0;
    point at = {}, home = {};
    uint32_t heading = 0, leg = 0;
    bool returning = false;
    Array<uint8_t> steps;
    Array<uint64_t> path, perimeter;
    gtl::flat_hash_map<uint64_t, uint32_t> per_site;
    Array<walker> walkers;
    // one bit per key: set while some walker owns the key, so the table must hold it
    Array<uint64_t> shadow, seen;
    Table t;

    static uint64_t value_of(const T& x)
    {
        if constexpr (is_tracked)
            return x.v;
        else
            return uint64_t{x};
    }

    const T* table_find(uint64_t k) const
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

    bool table_insert(uint64_t k, T value)
    {
        if constexpr (Table::dims == 1)
            return t.insert(k, move(value));
        else
            return at_coords<Table>(k, [&](auto... c) { return t.insert(c..., move(value)); });
    }

    T table_erase(uint64_t k)
    {
        if constexpr (Table::dims == 1)
            return t.erase(k);
        else
            return at_coords<Table>(k, [&](auto... c) { return t.erase(c...); });
    }

    static void hook(uint32_t action, uint64_t key)
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

    static void on_destroy(uint64_t v)
    {
        random_run& r = *current;
        if (const auto it = r.where.find(v); it != r.where.end())
        {
            const uint64_t k = it->second;
            r.where.erase(it);
            r.check_bucket(k);
        }
        fm_assert(r.where.size() == tracked::live);
        if constexpr (free_empty)
            fm_assert(r.t.size() == tracked::live);
        const uint64_t cost = (uint64_t{1} << Table::top_bits) + (uint64_t{r.t.page_count()} << page_bits);
        if (++r.destroyed * walk_budget >= cost)
        {
            r.destroyed = 0;
            r.check_where(false);
        }
    }

    void check_bucket(uint64_t k)
    {
        const auto top = t.raw_top();
        if constexpr (!Table::has_pages)
            fm_assert(!top[k]);
        else if constexpr (page_bits <= 10)
        {
            const uint64_t ti = k >> (page_bits + zero_bits), zi = k >> page_bits & zero_mask;
            const auto& e = top[ti];
            if constexpr (zero_bits > 0)
                if (e.side)
                {
                    uint32_t side_refs = 0;
                    for (uint32_t z = 1; z <= zero_mask; z++)
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
            const uint64_t base = k >> page_bits << page_bits;
            uint32_t n = 0;
            for (uint32_t i = 0; i < 1u << page_bits; i++)
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
        const uint64_t n = check_layers(t, [&](uint64_t k, const T& x) {
            if (!keys)
                return;
            const auto it = where.find(value_of(x));
            fm_assert(it != where.end() && it->second == k);
        });
        fm_assert(n == where.size() && n == tracked::live);
    }

    uint64_t next_plain()
    {
        const uint64_t v = ++serial;
        if constexpr (!is_tracked && sizeof(T) < sizeof(uint64_t))
            fm_assert(v <= std::numeric_limits<T>::max());
        return v;
    }

    uint64_t window_key(uint32_t i) const
    {
        return bases[i / p.window_size] + i % p.window_size;
    }

    void make_bases()
    {
        const uint32_t w = p.window_size;
        fm_assert(p.windows >= 2 && w > 0);
        if constexpr (Table::has_pages)
            fm_assert(w <= 1u << (page_bits - 1));
        const uint64_t last = key_mask - (w - 1);
        arrayAppend(bases, uint64_t{0});
        arrayAppend(bases, last);
        for (uint32_t i = 2; i < p.windows; i++)
        {
            const uint32_t shift = page_bits + (i % 2 ? zero_bits : 0);
            const uint64_t b = i == 2 ? uint64_t{1} << shift : (Random::next(rng) & key_mask) >> shift << shift;
            const uint64_t base = b < w / 2 ? 0 : b - w / 2;
            arrayAppend(bases, base < last ? base : last);
        }
    }

    uint64_t present_key()
    {
        return keys[below(rng, (uint32_t)keys.size())];
    }

    uint64_t fresh_key()
    {
        if (p.mode == key_mode::walk)
            return window_key(below(rng, p.windows * p.window_size));
        return Random::next(rng) & key_mask;
    }

    uint64_t pick_key(bool ins)
    {
        if (p.mode == key_mode::walk)
        {
            const uint32_t n = p.windows * p.window_size;
            const uint64_t r = Random::next(rng);
            if (r % 64 == 0)
                walk_pos = uint32_t((r >> 32) * n >> 32);
            else
                walk_pos = (walk_pos + n - 8 + uint32_t(r >> 8) % 17) % n;
            return window_key(walk_pos);
        }
        if constexpr (Table::key_bits > 16)
            if (!keys.isEmpty() && Random::next(rng) % 8 < (ins ? 1u : 6u))
                return present_key();
        return fresh_key();
    }

    uint64_t make_value(uint64_t k)
    {
        if constexpr (is_tracked)
            if (p.hooks && Random::next(rng) % 8 == 0)
            {
                const uint64_t r = Random::next(rng);
                const uint32_t action = r % 64 == 0 ? act_clear : r >> 6 & 1 ? act_erase : act_insert;
                const uint64_t which = r >> 7 & 7;
                const uint64_t key = which == 0 ? k : which < 4 && !keys.isEmpty() ? present_key() : fresh_key();
                return act(action, key, serial++ & ((uint64_t{1} << (61 - tracked::key_bits)) - 1));
            }
        return next_plain();
    }

    void ref_add(uint64_t k, uint64_t v, const T* slot)
    {
        fm_assert(slot);
        const bool added = ref.try_emplace(k, entry{v, slot, (uint32_t)keys.size()}).second;
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

    void ref_remove(uint64_t k)
    {
        const auto it = ref.find(k);
        fm_assert(it != ref.end());
        const uint32_t i = it->second.pos, last = (uint32_t)keys.size() - 1;
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
    void insert(uint64_t k, uint64_t v)
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

    void erase(uint64_t k)
    {
        uint64_t expected = 0;
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
        for (uint64_t& x : shadow)
            x = 0;
        for (walker& w : walkers)
        {
            arrayClear(w.path);
            w.returning = false;
        }
    }

    void check_key(uint64_t k) const
    {
        const T* s = table_find(k);
        if (const auto it = ref.find(k); it != ref.end())
            fm_assert(s == it->second.slot && value_of(*s) == it->second.v);
        else
            fm_assert(!s || !*s);
    }

    void check_absent()
    {
        const auto absent = [&](uint64_t k) {
            if (!ref.contains(k))
                fm_assert(!present(t, k));
        };
        if (p.mode == key_mode::walk)
        {
            for (uint32_t i = 0; i < p.windows * p.window_size; i++)
                absent(window_key(i));
        }
        else if constexpr (Table::key_bits <= 18)
        {
            for (uint64_t k = 0; k <= key_mask; k++)
                absent(k);
        }
        else
        {
            for (uint64_t k : keys)
            {
                if (k > 0)
                    absent(k - 1);
                if (k < key_mask)
                    absent(k + 1);
            }
            for (uint32_t i = 0; i < 256; i++)
                absent(Random::next(rng) & key_mask);
        }
    }

    void check()
    {
        const auto top = t.raw_top();
        uint64_t n = 0;
        const auto visit = [&](uint64_t k, const T& x) {
            const auto it = ref.find(k);
            fm_assert(it != ref.end() && it->second.v == value_of(x) && it->second.slot == &x);
            n++;
        };
        if constexpr (!Table::has_pages)
        {
            for (uint32_t k = 0; k < top.size(); k++)
                if (top[k])
                    visit(k, top[k]);
        }
        else
        {
            fm_assert(t.page_count() == buckets.size());
            for (const auto& rec : t.raw_pages())
            {
                const uint64_t bucket = uint64_t{rec.top_index} << zero_bits | rec.zero_index;
                uint32_t live = 0;
                for (uint32_t i = 0; i < 1u << page_bits; i++)
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
                uint32_t num_sides = 0;
                for (uint32_t i = 0; i < top.size(); i++)
                    if (top[i].side)
                    {
                        const auto s = sides.find(i);
                        fm_assert(s != sides.end());
                        if constexpr (free_empty)
                            fm_assert(top[i].side[0].live == s->second);
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

    bool phase_insert(uint32_t op)
    {
        const uint32_t insert_fifths = op / 2000 % 2 ? 1 : 4;
        return Random::next(rng) % 5 < insert_fifths && keys.size() < p.live_cap;
    }

    void touch(uint64_t k, bool ins)
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
            const uint64_t k = present_key();
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

    uint64_t key_at(const point& s) const
    {
        if constexpr (dims == 1)
            return s[0];
        else
            return test_pack<params_of<Table>>({uint32_t(s[0]), uint32_t(s[1]), uint32_t(s[2])});
    }

    point point_of(uint64_t k) const
    {
        if constexpr (dims == 1)
            return {k, 0, 0};
        else
        {
            const coord3 c = test_unpack<params_of<Table>>(k);
            return {c[0], c[1], c[2]};
        }
    }

    static point moved(point s, uint32_t dir, uint64_t n = 1)
    {
        const uint32_t d = dir / 2;
        s[d] = (dir & 1 ? s[d] - n : s[d] + n) & low_mask(site_bits[d]);
        return s;
    }

    static uint64_t torus_distance(const point& a, const point& b)
    {
        uint64_t n = 0;
        for (uint32_t d = 0; d < dims; d++)
        {
            const uint64_t m = low_mask(site_bits[d]), x = (a[d] - b[d]) & m, y = (b[d] - a[d]) & m;
            n += x < y ? x : y;
        }
        return n;
    }

    point random_point()
    {
        point s = {};
        for (uint32_t d = 0; d < dims; d++)
            s[d] = Random::next(rng) & low_mask(site_bits[d]);
        return s;
    }

    point page_corner()
    {
        point s = random_point();
        for (uint32_t d = 0; d < dims; d++)
            s[d] &= ~low_mask(box[d]);
        return s;
    }

    uint32_t pick_weighted(const uint32_t (&w)[6])
    {
        uint32_t sum = 0, i = 0;
        for (uint32_t dir = 0; dir < dirs; dir++)
            sum += w[dir];
        for (uint32_t r = below(rng, sum); r >= w[i]; i++)
            r -= w[i];
        return i;
    }

    uint32_t visits(uint64_t k) const
    {
        const auto it = per_site.find(k);
        return it != per_site.end() ? it->second : 0;
    }

    uint32_t grow_cap() const
    {
        if constexpr (dims == 1)
            return 24;
        else
            return key_mask / 4 < 256 ? uint32_t(key_mask / 4 + 1) : 256;
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
            shadow = Array<uint64_t>{ValueInit, size_t(key_mask >> 6) + 1};
            seen = Array<uint64_t>{ValueInit, size_t(key_mask >> 6) + 1};
            walkers = Array<walker>{ValueInit, p.walkers};
            for (walker& w : walkers)
                w.at = random_point();
        }
        home = at;
    }

    void kinetic_growth_step()
    {
        if (!ref.contains(key_at(at)))
        {
            touch(key_at(at), true);
            return;
        }
        uint32_t open[6], n = 0;
        for (uint32_t dir = 0; dir < dirs; dir++)
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
        const uint64_t k = key_at(at);
        if (const auto it = per_site.find(k); it != per_site.end())
        {
            const uint32_t keep = it->second + 1;
            while (path.size() > keep)
            {
                const uint64_t j = path.back();
                arrayRemoveSuffix(path, 1);
                per_site.erase(j);
                touch(j, false);
            }
        }
        else
        {
            per_site[k] = (uint32_t)path.size();
            arrayAppend(path, k);
            touch(k, true);
        }
    }

    void langton_ant_step()
    {
        const uint64_t k = dims == 1 ? at[0] | at[1] << ant_x_bits : key_at(at);
        const bool black = ref.contains(k);
        heading = (heading + (black ? 3 : 1)) % 4;
        touch(k, !black);
        const uint32_t d = heading % 2;
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
            const uint32_t i = below(rng, (uint32_t)perimeter.size());
            const uint64_t k = perimeter[i];
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
        for (uint32_t dir = 0; dir < dirs; dir++)
        {
            const uint64_t k = key_at(moved(at, dir));
            if (!ref.contains(k) && per_site.try_emplace(k, (uint32_t)perimeter.size()).second)
                arrayAppend(perimeter, k);
        }
    }

    static bool bit(const Array<uint64_t>& a, uint64_t k)
    {
        return (a[k >> 6] >> (k & 63) & 1) != 0;
    }

    static void flip(Array<uint64_t>& a, uint64_t k)
    {
        a[k >> 6] ^= uint64_t{1} << (k & 63);
    }

    void retrace_visit(walker& w)
    {
        const uint64_t k = key_at(w.at);
        const bool own = !bit(shadow, k);
        fm_assert(own == !ref.contains(k));
        if (own)
        {
            flip(shadow, k);
            touch(k, true);
        }
        else
            check_key(k);
        arrayAppend(w.path, k << 1 | uint64_t{own});
    }

    void retrace_back(walker& w)
    {
        const uint64_t e = w.path.back(), k = e >> 1;
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
        walker& w = walkers[below(rng, (uint32_t)walkers.size())];
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
            w.leg = 64 + below(rng, 961);
            if (walkers.size() > 1 && below(rng, 4) == 0)
                w.at = walkers[below(rng, (uint32_t)walkers.size())].at;
            retrace_visit(w);
        }
        w.at = moved(w.at, below(rng, dirs));
        retrace_visit(w);
        w.returning = w.path.size() > w.leg;
    }

    void retrace_finish()
    {
        Array<uint32_t> busy;
        for (uint32_t i = 0; i < walkers.size(); i++)
            if (!walkers[i].path.isEmpty())
                arrayAppend(busy, i);
        while (!busy.isEmpty())
        {
            const uint32_t i = below(rng, (uint32_t)busy.size());
            walker& w = walkers[busy[i]];
            retrace_back(w);
            if (w.path.isEmpty())
            {
                busy[i] = busy.back();
                arrayRemoveSuffix(busy, 1);
            }
        }
        fm_assert(keys.isEmpty());
        for (uint64_t x : shadow)
            fm_assert(!x);
        drain();
    }

    void check_owners()
    {
        for (uint64_t k = 0; k <= key_mask; k++)
            fm_assert(bit(shadow, k) == ref.contains(k));
        for (uint64_t& x : seen)
            x = 0;
        uint64_t owned = 0;
        for (const walker& w : walkers)
            for (uint64_t e : w.path)
                if (e & 1)
                {
                    const uint64_t k = e >> 1;
                    fm_assert(bit(shadow, k) && !bit(seen, k));
                    flip(seen, k);
                    owned++;
                }
        fm_assert(owned == keys.size());
    }

    void walk_step(uint32_t op)
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
            const uint32_t dir = below(rng, dirs);
            const double u = double((Random::next(rng) >> 11) + 1) * 0x1p-53;
            at = moved(at, dir, uint64_t(std::pow(u, -1 / 1.5)));
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
            uint32_t w[6] = {};
            for (uint32_t dir = 0; dir < dirs; dir++)
            {
                const uint32_t n = visits(key_at(moved(at, dir)));
                w[dir] = 1u << (20 - (n < 20 ? n : 20));
            }
            at = moved(at, pick_weighted(w));
            per_site[key_at(at)]++;
            break;
        }
        case walk_type::loop_erased:
            loop_erased_step();
            return;
        case walk_type::vertex_reinforced: {
            uint32_t w[6] = {};
            for (uint32_t dir = 0; dir < dirs; dir++)
                w[dir] = 1 + visits(key_at(moved(at, dir)));
            at = moved(at, pick_weighted(w));
            per_site[key_at(at)]++;
            touch(key_at(at), !ref.contains(key_at(at)));
            return;
        }
        case walk_type::elephant: {
            uint32_t dir = below(rng, dirs);
            if (!steps.isEmpty())
            {
                const uint32_t past = steps[below(rng, (uint32_t)steps.size())];
                const uint32_t other = below(rng, dirs - 1);
                dir = below(rng, 10) < 9 ? past : other + (other >= past);
            }
            arrayAppend(steps, uint8_t(dir));
            at = moved(at, dir);
            break;
        }
        case walk_type::resetting: {
            if (below(rng, 64) == 0)
                at = home;
            else
                at = moved(at, below(rng, dirs));
            const uint32_t insert_fifths = torus_distance(at, home) <= 3 ? 4 : 1;
            touch(key_at(at), Random::next(rng) % 5 < insert_fifths && keys.size() < p.live_cap);
            return;
        }
        case walk_type::random_environment: {
            // each site's bias is 4:1 up or down per dimension, with E[log ratio] = 0
            const uint32_t d = below(rng, dims);
            const bool up = (mix64(key_at(at) ^ p.seed) >> d & 1) != 0;
            const bool plus = below(rng, 5) < (up ? 4u : 1u);
            at = moved(at, d * 2 + !plus);
            break;
        }
        case walk_type::excited: {
            const bool first = per_site.try_emplace(key_at(at), 1u).second;
            uint32_t dir = below(rng, dirs);
            if (first && dir == 1 && (Random::next(rng) & 1))
                dir = 0;
            at = moved(at, dir);
            break;
        }
        case walk_type::langton_ant:
            langton_ant_step();
            return;
        case walk_type::rotor_router: {
            const uint64_t k = key_at(at);
            const auto it = per_site.try_emplace(k, uint32_t(mix64(k ^ p.seed) % dirs)).first;
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
            const uint32_t d = below(rng, dims);
            const uint64_t m = low_mask(box[d]), o = at[d] & m;
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
    explicit random_run(const random_params& p) : p{p} { Random::seed(rng, p.seed); }

    void run()
    {
        current = this;
        tracked::hook = hook;
        tracked::on_destroy = on_destroy;
        if (p.mode == key_mode::walk)
            make_bases();
        if (p.walk != walk_type::none)
            walk_begin();
        for (uint32_t op = 1; op <= p.ops; op++)
        {
            if (p.walk != walk_type::none)
                walk_step(op);
            else
            {
                const bool ins = phase_insert(op);
                const uint64_t k = pick_key(ins);
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
    random_run<Table>{p}.run();
}

template<typename Table>
void check_walk(uint64_t seed, walk_type walk, uint32_t ops = 3000)
{
    check_random<Table>({.seed = seed, .ops = ops, .top_size_bytes = top_bytes<Table>, .walk = walk,
                         .hooks = walk != walk_type::retrace, .check_every = 500, .move_every = 1000});
}

// each walker makes about two round trips
template<typename Table>
void check_retrace(uint64_t seed, uint32_t walkers)
{
    check_random<Table>({.seed = seed, .ops = walkers * 2200, .top_size_bytes = top_bytes<Table>,
                         .walk = walk_type::retrace, .walkers = walkers, .check_every = 500, .move_every = 1000});
}

constexpr random_params with(uint64_t seed, size_t top_size_bytes)
{
    return { .seed = seed, .ops = 8000, .top_size_bytes = top_size_bytes, .hooks = true,
             .check_every = 512, .clear_every = 2000, .move_every = 1000 };
}

template<typename Table>
void check_fill_block(uint64_t block, Random::xoshiro256starstar& rng)
{
    using T = std::remove_cvref_t<decltype(*std::declval<const Table&>().find(0))>;
    constexpr bool free_empty = requires (const Table& t) { t.size(); };
    constexpr uint32_t bits = Table::has_pages ? Table::page_bits : Table::key_bits;
    constexpr uint32_t n = 1u << bits, zero_bits = Table::zero_bits;
    constexpr uint64_t last_block = (uint64_t{1} << (Table::key_bits - bits)) - 1;
    const uint64_t base = block << bits;
    const auto value = [](uint64_t k) {
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
    const auto key_of = [&](uint32_t i) -> uint64_t {
        if constexpr (Table::dims == 1)
            return base | i;
        else
        {
            coord3 c = origin;
            for (uint32_t d = 0, shift = 0; d < Table::dims; shift += box[d], d++)
                c[d] += i >> shift & ((1u << box[d]) - 1);
            return test_pack<params_of<Table>>(c);
        }
    };
    const auto insert = [&](uint64_t k) {
        if constexpr (Table::dims == 1)
            return t.insert(k, value(k));
        else
            return at_coords<Table>(k, [&](auto... c) { return t.insert(c..., value(k)); });
    };
    const auto erase = [&](uint64_t k) {
        if constexpr (Table::dims == 1)
            return t.erase(k);
        else
            return at_coords<Table>(k, [&](auto... c) { return t.erase(c...); });
    };

    for (uint32_t i = 0; i < n; i++)
        fm_assert(insert(key_of(i)));
    const uint64_t found = check_layers(t, [&](uint64_t k, const T&) { fm_assert(k >> bits == block); });
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
        for (uint32_t d = 0; d < Table::dims; d++)
        {
            coord3 c = origin;
            if (origin[d] > 0)
            {
                c[d] = origin[d] - 1;
                absent(c);
            }
            if ((uint64_t{origin[d]} + (uint64_t{1} << box[d])) >> dim_bits[d] == 0)
            {
                c[d] = origin[d] + (1u << box[d]);
                absent(c);
            }
        }
    }

    [[maybe_unused]] const T* page = nullptr;
    [[maybe_unused]] uint32_t ti = 0, zi = 0;
    if constexpr (Table::has_pages)
    {
        fm_assert(t.page_count() == 1);
        const auto rec = t.raw_pages()[0];
        page = rec.page;
        ti = rec.top_index;
        zi = rec.zero_index;
        fm_assert((uint64_t{ti} << zero_bits | zi) == block);
        const auto top = t.raw_top();
        if constexpr (zero_bits > 0)
            for (uint32_t i = 0; i < top.size(); i++)
                fm_assert(!top[i].side == (i != ti || !zi));
        if constexpr (free_empty)
            fm_assert(ref_at<zero_bits>(top, ti, zi).live == n && t.size() == n);
    }

    Array<uint32_t> order{NoInit, n};
    for (uint32_t i = 0; i < n; i++)
    {
        const uint32_t j = below(rng, i + 1);
        if (j != i)
            order[i] = order[j];
        order[j] = i;
    }
    for (uint32_t i = 0; i < n; i++)
    {
        const T got = erase(key_of(order[i]));
        fm_assert(got);
        if constexpr (free_empty)
        {
            if (i + 1 < n)
                fm_assert(ref_at<zero_bits>(t.raw_top(), ti, zi).live == n - i - 1);
        }
    }
    fm_assert(check_layers(t, [](uint64_t, const T&) {}) == 0);
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
void check_fill(uint64_t seed)
{
    constexpr uint32_t bits = Table::has_pages ? Table::page_bits : Table::key_bits;
    constexpr uint64_t last_block = (uint64_t{1} << (Table::key_bits - bits)) - 1;
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
void check_pack(uint64_t seed)
{
    constexpr coord3 box = box_bits(params_of<Table>), dim_bits = coord_bits(params_of<Table>);
    constexpr uint32_t block_bits = Table::has_pages ? Table::page_bits : Table::key_bits;
    constexpr uint64_t block_mask = (uint64_t{1} << block_bits) - 1;
    constexpr uint64_t key_mask = ~uint64_t{0} >> (64 - Table::key_bits);
    gtl::flat_hash_map<uint64_t, coord3> block_high;
    const auto check_key = [&](uint64_t k) {
        const coord3 c = test_unpack<params_of<Table>>(k);
        coord3 high = {};
        uint64_t slot = 0;
        for (uint32_t d = 0, shift = 0; d < 3; shift += box[d], d++)
        {
            fm_assert(!(uint64_t{c[d]} >> dim_bits[d]));
            slot |= (uint64_t{c[d]} & ((uint64_t{1} << box[d]) - 1)) << shift;
            high[d] = uint32_t(uint64_t{c[d]} >> box[d]);
        }
        fm_assert(slot == (k & block_mask));
        fm_assert(block_high.try_emplace(k >> block_bits, high).first->second == high);
        at_coords<Table>(k, [](auto...) {});
    };
    if constexpr (Table::key_bits <= 16)
    {
        for (uint64_t k = 0; k <= key_mask; k++)
            check_key(k);
    }
    else
    {
        Random::xoshiro256starstar rng;
        Random::seed(rng, seed);
        for (uint32_t i = 0; i < 4096; i++)
            check_key(Random::next(rng) & key_mask);
        for (uint32_t m = 0; m < 1u << Table::dims; m++)
        {
            coord3 c = {};
            for (uint32_t d = 0; d < Table::dims; d++)
                if (m >> d & 1)
                    c[d] = uint32_t((uint64_t{1} << dim_bits[d]) - 1);
            check_key(test_pack<params_of<Table>>(c));
        }
    }
}

} // namespace floormat::mlt_test
