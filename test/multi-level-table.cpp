#include "app.hpp"
#include "compat/defs.hpp"
#include "compat/multi-level-table.inl"
#include "compat/borrowed-ptr.inl"
#include "random/xoshiro256starstar.hpp"
#include "src/chunk-table.hpp"
#include "src/object-table.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/light.hpp"
#include "src/global-coords.hpp"
#include "src/tile-defs.hpp"
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

// Outside the anonymous namespace: GCC warns about members that an explicit
// instantiation with internal linkage never calls.
namespace mlt_test {

struct tracked
{
    static constexpr uint64_t act_flag = uint64_t{1} << 63;
    static constexpr uint32_t key_bits = 40;
    static inline uint32_t live = 0;
    static inline void (*hook)(uint32_t action, uint64_t key) = nullptr;
    static inline void (*on_destroy)(uint64_t v) = nullptr;

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

} // namespace mlt_test

namespace {

using mlt_test::tracked;

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
    .levels = { {.bits = {2, 1}}, {.bits = {3, 1}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params plane_deep_free_params = mlt_params{
    .levels = { {.bits = {3, 0}}, {.bits = {0, 2}}, {.bits = {2, 1}, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params plane_no_y_params = mlt_params{
    .levels = { {.bits = {3, 0}}, {.bits = {2, 0}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params plane_zero_params = mlt_params{
    .levels = { {.bits = {1, 1}}, {.bits = {1, 0}, .inline_zero = true}, {.bits = {2, 1}, .dynamic = true}, },
}.validate();

constexpr inline mlt_params plane_zero_free_params = mlt_params{
    .levels = { {.bits = {1, 1}}, {.bits = {1, 0}, .inline_zero = true}, {.bits = {2, 1}, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params plane_no_pages_params = mlt_params{
    .levels = { {.bits = {3, 2}}, },
}.validate();

constexpr inline mlt_params plane_wide_params = mlt_params{
    .levels = { {.bits = {16, 0}}, {.bits = {16, 1}, .dynamic = true}, },
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
    .levels = { {.bits = {2, 2}}, {.bits = {1, 1}, .inline_zero = true}, {.bits = {3, 3}, .dynamic = true}, },
    .free_empty = true,
}.validate();

constexpr inline mlt_params space_large_params = mlt_params{
    .levels = { {.bits = {1, 1, 1}}, {.bits = {1, 1, 0}, .inline_zero = true}, {.bits = {2, 2, 2}, .dynamic = true}, },
    .free_empty = true,
}.validate();

// a failing validate() is not a constant expression, which leaves the requirement unsatisfied
template<mlt_params P>
concept valid_params = requires { typename std::integral_constant<uint32_t, P.validate().depth()>; };

static_assert(valid_params<mlt_params{ .levels = { {.bits = 1}, } }>);
static_assert(valid_params<mlt_params{ .levels = { {.bits = 1, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{}>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 3}, {.bits = 0}, {.bits = 4, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 3}, {.bits = 0, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 3}, {.bits = 0, .inline_zero = true}, {.bits = 4, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 4, .dynamic = true}, {.bits = 4}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 2, .inline_zero = true}, {.bits = 2}, {.bits = 2, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 2, .inline_zero = true}, {.bits = 2}, } }>);
static_assert(valid_params<mlt_params{ .levels = { {.bits = 2}, {.bits = 2, .inline_zero = true}, {.bits = 2, .dynamic = true}, }, .free_empty = true }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 4}, }, .free_empty = true }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 4}, }, .page_source = mlt_source::superpage }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 31}, {.bits = 31}, {.bits = 3, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 32}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 1}, {.bits = 32, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = 1}, {.bits = 32, .inline_zero = true}, {.bits = 1, .dynamic = true}, } }>);
static_assert(valid_params<mlt_params{ .levels = { {.bits = {2, 1}}, {.bits = {3, 1}, .dynamic = true}, } }>);
static_assert(valid_params<mlt_params{ .levels = { {.bits = {16, 0}}, {.bits = {16, 1}, .dynamic = true}, } }>);
static_assert(valid_params<mlt_params{ .levels = { {.bits = {1, 0, 2}}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = {2, 1}}, {.bits = 4, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = {2, 1, 0}}, {.bits = {3, 1}, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = {17, 0}}, {.bits = {16, 1}, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = {0, 0}}, {.bits = {3, 1}, .dynamic = true}, } }>);

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
    if constexpr (Table::dims == 2)
    {
        fm_assert(Table::pack(c[0], c[1]) == k);
        return f(c[0], c[1]);
    }
    else
    {
        fm_assert(Table::pack(c[0], c[1], c[2]) == k);
        return f(c[0], c[1], c[2]);
    }
}

template<typename Table>
constexpr bool packs_to(coord3 c, uint64_t k)
{
    const uint64_t table_key = [&] {
        if constexpr (Table::dims == 2)
            return Table::pack(c[0], c[1]);
        else
            return Table::pack(c[0], c[1], c[2]);
    }();
    return table_key == k && test_pack<params_of<Table>>(c) == k && test_unpack<params_of<Table>>(k) == c;
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
using chunk_table_mlt = multi_level_table<chunk*, detail::chunk_table_params>;

template<typename Table>
constexpr bool move_only = !std::is_copy_constructible_v<Table> && !std::is_copy_assignable_v<Table> &&
                           std::is_nothrow_move_constructible_v<Table> && std::is_nothrow_move_assignable_v<Table>;

static_assert(move_only<chunk_like_table> && move_only<id_like_table> && move_only<split_table> &&
              move_only<flat_table> && move_only<deep_table> && move_only<no_pages_table> &&
              move_only<one_split_table> && move_only<one_entry_table> && move_only<wide_table>);
static_assert(move_only<object_table> && move_only<chunk_table_mlt>);

template<typename Table>
constexpr size_t top_bytes = sizeof(typename decltype(std::declval<const Table&>().raw_top())::Type) << Table::top_bits;

static_assert(top_bytes<object_table> == 2 << 20 && sizeof(bptr<object>) << object_table::page_bits == 2 << 20);
static_assert(top_bytes<chunk_table_mlt> == 2 << 20 && sizeof(chunk*) << chunk_table_mlt::page_bits == 2 << 20);

static_assert(chunk_table_mlt::dims == 3 && chunk_table_mlt::key_bits == 36);
static_assert(packs_to<chunk_table_mlt>({1, 0, 0}, 1) && packs_to<chunk_table_mlt>({0, 1, 0}, 1 << 9));
static_assert(packs_to<chunk_table_mlt>({1 << 9, 0, 0}, uint64_t{1} << 22) && packs_to<chunk_table_mlt>({0, 1 << 9, 0}, uint64_t{1} << 29));
static_assert(packs_to<chunk_table_mlt>({0, 0, 1}, 1 << 18) && packs_to<chunk_table_mlt>({0xffff, 0xffff, 15}, (uint64_t{1} << 36) - 1));

static_assert(plane_table::dims == 2 && plane_table::key_bits == 7);
static_assert(packs_to<plane_table>({1, 0, 0}, 1) && packs_to<plane_table>({0, 1, 0}, 8) && packs_to<plane_table>({8, 0, 0}, 16));
static_assert(packs_to<plane_table>({0, 2, 0}, 64) && packs_to<plane_table>({31, 3, 0}, 127) && packs_to<plane_table>({5, 2, 0}, 69));
static_assert(packs_to<plane_deep_free_table>({0, 1, 0}, 4) && packs_to<plane_deep_free_table>({0, 2, 0}, 8) && packs_to<plane_deep_free_table>({0, 4, 0}, 16));
static_assert(packs_to<plane_deep_free_table>({4, 0, 0}, 32) && packs_to<plane_deep_free_table>({31, 7, 0}, 255));
static_assert(packs_to<plane_zero_table>({4, 0, 0}, 8) && packs_to<plane_zero_table>({8, 0, 0}, 16) && packs_to<plane_zero_table>({0, 2, 0}, 32));
static_assert(packs_to<plane_wide_table>({0, 1, 0}, 1 << 16) && packs_to<plane_wide_table>({0x10000, 0, 0}, 1 << 17));
static_assert(packs_to<plane_wide_table>({0xffffffff, 1, 0}, (uint64_t{1} << 33) - 1));
static_assert(space_deep_table::dims == 3 && space_deep_table::key_bits == 12);
static_assert(packs_to<space_deep_table>({1, 0, 0}, 1) && packs_to<space_deep_table>({0, 1, 0}, 8) && packs_to<space_deep_table>({0, 0, 1}, 64));
static_assert(packs_to<space_deep_table>({8, 0, 0}, 256) && packs_to<space_deep_table>({0, 8, 0}, 1024) && packs_to<space_deep_table>({31, 31, 3}, 4095));
static_assert(packs_to<space_free_table>({0, 1, 0}, 4) && packs_to<space_free_table>({4, 0, 0}, 8) && packs_to<space_free_table>({0, 2, 0}, 16));
static_assert(packs_to<space_free_table>({0, 0, 1}, 32));
static_assert(packs_to<space_zero_free_table>({0, 0, 1}, 16) && packs_to<space_zero_free_table>({0, 0, 2}, 32) && packs_to<space_zero_free_table>({4, 4, 0}, 192));
static_assert(packs_to<space_one_entry_table>({1, 0, 0}, 1) && packs_to<space_one_entry_table>({0, 0, 1}, 2));
static_assert(packs_to<plane_large_table>({1, 0, 0}, 1) && packs_to<plane_large_table>({0, 1, 0}, 8) && packs_to<plane_large_table>({8, 0, 0}, 64));
static_assert(packs_to<plane_large_table>({0, 8, 0}, 128) && packs_to<plane_large_table>({16, 0, 0}, 256) && packs_to<plane_large_table>({0, 16, 0}, 1024));
static_assert(packs_to<plane_large_table>({63, 63, 0}, 4095) && top_bytes<plane_large_table> == 24 << 4);
static_assert(packs_to<space_large_table>({0, 0, 1}, 16) && packs_to<space_large_table>({4, 0, 0}, 64) && packs_to<space_large_table>({0, 4, 0}, 128));
static_assert(packs_to<space_large_table>({8, 0, 0}, 256) && packs_to<space_large_table>({0, 8, 0}, 512) && packs_to<space_large_table>({0, 0, 4}, 1024));
static_assert(packs_to<space_large_table>({15, 15, 7}, 2047) && top_bytes<space_large_table> == 24 << 3);

} // namespace

template class multi_level_table<uint64_t, chunk_like_params>;
template class multi_level_table<tracked, id_like_params>;
template class multi_level_table<uint32_t, split_params>;
template class multi_level_table<uint16_t, flat_params>;
template class multi_level_table<uint64_t, deep_params>;
template class multi_level_table<tracked, deep_params>;
template class multi_level_table<int8_t, byte_params>;
template class multi_level_table<uint8_t, byte_params>;
template class multi_level_table<int16_t, short_params>;
template class multi_level_table<uint16_t, short_params>;
template class multi_level_table<int64_t, id_like_params>;
template class multi_level_table<uint64_t, id_like_params>;
template class multi_level_table<tracked, no_pages_params>;
template class multi_level_table<tracked, one_split_params>;
template class multi_level_table<tracked, bit_page_params>;
template class multi_level_table<tracked, bit_flat_params>;
template class multi_level_table<uint32_t, bit_flat_params>;
template class multi_level_table<tracked, bit_pair_params>;
template class multi_level_table<tracked, bit_split_params>;
template class multi_level_table<tracked, bit_triple_params>;
template class multi_level_table<tracked, bit_deep_params>;
template class multi_level_table<tracked, wide_top_params>;
template class multi_level_table<tracked, narrow_top_params>;
template class multi_level_table<tracked, bit_zero_params>;
template class multi_level_table<tracked, wide_zero_params>;
template class multi_level_table<tracked, zero_top_params>;
template class multi_level_table<tracked, split_free_params>;
template class multi_level_table<tracked, bit_split_free_params>;
template class multi_level_table<uint64_t, one_entry_params>;
template class multi_level_table<uint8_t, wide_params>;
template class multi_level_table<tracked, plane_params>;
template class multi_level_table<tracked, plane_deep_free_params>;
template class multi_level_table<tracked, plane_no_y_params>;
template class multi_level_table<tracked, plane_zero_params>;
template class multi_level_table<tracked, plane_zero_free_params>;
template class multi_level_table<tracked, plane_no_pages_params>;
template class multi_level_table<uint8_t, plane_wide_params>;
template class multi_level_table<tracked, space_free_params>;
template class multi_level_table<tracked, space_deep_params>;
template class multi_level_table<uint32_t, space_deep_params>;
template class multi_level_table<tracked, space_zero_free_params>;
template class multi_level_table<tracked, space_no_pages_params>;
template class multi_level_table<tracked, space_one_entry_params>;
template class multi_level_table<tracked, plane_large_params>;
template class multi_level_table<tracked, space_large_params>;

namespace {

template<typename Table, typename T>
T value_at(const Table& t, uint64_t key)
{
    const auto* s = t.find(key);
    fm_assert(s);
    return *s;
}

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

void check_heap_array_bounds()
{
    auto* p = new uint16_t[5]{};
    check_heap_array(p, 5);
    fm_assert(__asan_region_is_poisoned(p, 6 * sizeof(uint16_t)));
    fm_assert(!__asan_address_is_poisoned(p + 4));
    delete[] p;
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

template<typename Table>
struct edge_keys
{
    static constexpr uint64_t page = uint64_t{1} << Table::page_bits;
    static constexpr uint64_t keys[] = { 0, page - 1, page, page + 1, (uint64_t{1} << Table::key_bits) - 1, };
};

template<typename T, mlt_params P>
void check_ints(size_t top_size_bytes, ArrayView<const uint64_t> keys)
{
    using Table = multi_level_table<T, P>;
    using L = std::numeric_limits<T>;
    constexpr auto values = [] {
        if constexpr (std::is_signed_v<T>)
            return std::array<T, 4>{L::min(), L::max(), T(-1), T(1)};
        else
            return std::array<T, 2>{L::max(), T(1)};
    }();

    Table t;
    for (uint32_t i = 0; i < keys.size(); i++)
        fm_assert(t.insert(keys[i], values[i % values.size()]));
    check_layout(t, top_size_bytes, keys);
    for (uint32_t i = 0; i < keys.size(); i++)
    {
        fm_assert(*t.find(keys[i]) == values[i % values.size()]);
        fm_assert(!t.insert(keys[i], T(1)));
    }
    const auto num_pages = t.page_count();
    for (uint32_t i = 0; i < keys.size(); i++)
        fm_assert(t.erase(keys[i]) == values[i % values.size()]);
    fm_assert(t.page_count() == (P.free_empty ? 0 : num_pages));
    for (uint64_t k : keys)
        fm_assert(!present(t, k));
    check_layout(t, top_size_bytes, {});
}

void check_chunk_like()
{
    static_assert(chunk_like_table::key_bits == 36);
    constexpr uint64_t page = uint64_t{1} << chunk_like_table::page_bits;
    constexpr uint64_t k0 = 0, k1 = page - 1, k2 = page, k3 = (uint64_t{1} << 36) - 1;

    chunk_like_table t;
    fm_assert(!t.find(k0) && !t.find(k3));
    fm_assert(t.insert(k0, 11));
    fm_assert(t.insert(k1, 13));
    fm_assert(t.insert(k2, 17));
    fm_assert(t.insert(k3, 19));
    fm_assert(t.page_count() == 3);
    fm_assert(!t.insert(k1, 23));
    fm_assert((value_at<chunk_like_table, uint64_t>(t, k0)) == 11);
    fm_assert((value_at<chunk_like_table, uint64_t>(t, k1)) == 13);
    fm_assert((value_at<chunk_like_table, uint64_t>(t, k2)) == 17);
    fm_assert((value_at<chunk_like_table, uint64_t>(t, k3)) == 19);
    fm_assert(!present(t, k2 + 1));
    fm_assert(!t.find(2 * page));
    const uint64_t keys[] = { k0, k1, k2, k3 };
    check_layout(t, 2 << 20, keys);

    fm_assert(t.erase(k1) == 13);
    fm_assert(t.erase(k1) == 0);
    fm_assert(t.erase(2 * page) == 0);
    fm_assert(t.page_count() == 3);
    fm_assert(t.find(k1) && !*t.find(k1));

    t.clear();
    fm_assert(t.page_count() == 0);
    fm_assert(!t.find(k0) && !t.find(k2) && !t.find(k3));
    fm_assert(t.insert(k2, 29));
    fm_assert((value_at<chunk_like_table, uint64_t>(t, k2)) == 29);
}

void check_id_like()
{
    static_assert(id_like_table::key_bits == 35);
    constexpr uint64_t page = uint64_t{1} << id_like_table::page_bits;
    constexpr uint64_t mask = page - 1;
    constexpr uint64_t k0 = 1025, k1 = page + 7, k2 = (uint64_t{1} << 35) - 1, k3 = 5 * page + 3;

    fm_assert(tracked::live == 0);
    {
        id_like_table t;
        set_target(t);
        fm_assert(t.insert(k0, tracked{31}));
        fm_assert(t.insert(k1, tracked{37}));
        fm_assert(t.insert(k2, tracked{41}));
        fm_assert(t.page_count() == 3);
        fm_assert(tracked::live == 3);
        fm_assert(!t.insert(k0, tracked{43}));
        fm_assert(tracked::live == 3);
        fm_assert(t.size() == 3);
        fm_assert(t.find(k0)->v == 31 && t.find(k1)->v == 37 && t.find(k2)->v == 41);
        const uint64_t keys[] = { k0, k1, k2 };
        check_layout(t, 2 << 20, keys);

        const tracked* base1 = t.find(k1) - (k1 & mask);
        fm_assert(t.erase(k1).v == 37);
        fm_assert(tracked::live == 2);
        fm_assert(t.page_count() == 2);
        fm_assert(t.size() == 2);
        fm_assert(!t.find(k1));
        fm_assert(t.insert(k3, tracked{47}));
        fm_assert(t.find(k3) - (k3 & mask) == base1);
        fm_assert(!present(t, k1 - page + 5 * page));
        fm_assert(t.page_count() == 3);

        fm_assert(t.insert(k3 + 1, tracked{act(act_erase, k3 + 2)}));
        fm_assert(t.insert(k3 + 2, tracked{53}));
        fm_assert(t.erase(k3).v == 47);
        (void)t.erase(k3 + 1);
        fm_assert(!t.find(k3 + 2));
        fm_assert(t.page_count() == 2);
        fm_assert(tracked::live == 2);
        fm_assert(t.size() == 2);

        fm_assert(t.insert(k3, tracked{act(act_erase, 59)}));
        fm_assert(t.insert(59, tracked{act(act_erase, k3)}));
        fm_assert(t.insert(k2 - 1, tracked{act(act_erase, k0)}));
        fm_assert(tracked::live == 5);
        fm_assert(t.size() == 5);
        t.clear();
        fm_assert(tracked::live == 0);
        fm_assert(t.page_count() == 0);
        fm_assert(t.size() == 0);
        fm_assert(!t.find(k0) && !t.find(k2) && !t.find(k3));

        fm_assert(t.insert(k1, tracked{67}));
        fm_assert(t.insert(k0, tracked{71}));
        id_like_table t2{move(t)};
        set_target(t2);
        fm_assert(t2.find(k1)->v == 67 && t2.find(k0)->v == 71);
        fm_assert(t2.page_count() == 2);
        fm_assert(t2.size() == 2 && t.size() == 0);

        id_like_table t3;
        fm_assert(t3.insert(k2, tracked{73}));
        fm_assert(tracked::live == 3);
        t3 = move(t2);
        fm_assert(tracked::live == 2);
        fm_assert(!t3.find(k2));
        fm_assert(t3.find(k1)->v == 67);
    }
    fm_assert(tracked::live == 0);
    tracked::hook = nullptr;
}

void check_inline_zero()
{
    constexpr auto key = [](uint64_t top, uint64_t z, uint64_t p) { return top << 6 | z << 4 | p; };
    split_table t;
    fm_assert(t.insert(key(5, 0, 3), 79));
    fm_assert(!t.find(key(5, 1, 3)));
    fm_assert(t.insert(key(5, 2, 15), 83));
    fm_assert(!t.find(key(5, 1, 3)));
    fm_assert(!present(t, key(5, 2, 14)));
    fm_assert((value_at<split_table, uint32_t>(t, key(5, 0, 3))) == 79);
    fm_assert((value_at<split_table, uint32_t>(t, key(5, 2, 15))) == 83);
    fm_assert(t.insert(key(7, 3, 0), 89));
    fm_assert(t.page_count() == 3);
    const uint64_t keys[] = { key(5, 0, 3), key(5, 2, 15), key(7, 3, 0) };
    check_layout(t, size_t{16} << 3, keys);
    fm_assert(t.erase(key(5, 2, 15)) == 83);
    fm_assert(t.erase(key(6, 2, 1)) == 0);
    t.clear();
    fm_assert(t.page_count() == 0);
    fm_assert(!t.find(key(5, 0, 3)) && !t.find(key(5, 2, 15)) && !t.find(key(7, 3, 0)));
    fm_assert(t.insert(key(5, 2, 15), 97));
    fm_assert((value_at<split_table, uint32_t>(t, key(5, 2, 15))) == 97);
}

void check_flat()
{
    flat_table t;
    fm_assert(t.find(63) && !*t.find(63));
    fm_assert(t.insert(63, 101));
    fm_assert(t.insert(0, 103));
    fm_assert(!t.insert(63, 107));
    fm_assert(*t.find(63) == 101);
    const uint64_t keys[] = { 0, 63 };
    check_layout(t, size_t{2} << 6, keys);
    fm_assert(t.erase(0) == 103);
    fm_assert(t.page_count() == 0);
    flat_table t2{move(t)};
    fm_assert(*t2.find(63) == 101);
    fm_assert(t.raw_top().isEmpty());
}

void check_deep()
{
    static_assert(deep_table::key_bits == 12 && deep_table::top_bits == 7);
    deep_table t;
    fm_assert(t.insert(0, 109));
    fm_assert(t.insert(31, 113));
    fm_assert(t.insert(32, 127));
    fm_assert(t.insert(4095, 131));
    fm_assert(t.page_count() == 3);
    fm_assert(t.size() == 4);
    const uint64_t keys[] = { 0, 31, 32, 4095 };
    check_layout(t, size_t{16} << 7, keys);
    fm_assert(t.erase(31) == 113);
    fm_assert(t.page_count() == 3);
    fm_assert(t.size() == 3);
    fm_assert(t.erase(0) == 109);
    fm_assert(t.page_count() == 2);
    fm_assert(t.size() == 2);
    fm_assert(!t.find(0));
    fm_assert((value_at<deep_table, uint64_t>(t, 4095)) == 131);
    fm_assert(t.erase(4095) == 131);
    fm_assert(t.erase(32) == 127);
    fm_assert(t.page_count() == 0);
    fm_assert(t.size() == 0);
    fm_assert(t.insert(4064, 137));
    fm_assert((value_at<deep_table, uint64_t>(t, 4064)) == 137);
    fm_assert(t.size() == 1);
    t.clear();
    fm_assert(t.size() == 0);
}

void check_no_pages()
{
    constexpr uint64_t last = (uint64_t{1} << 18) - 1;
    fm_assert(tracked::live == 0);
    {
        no_pages_table t;
        set_target(t);
        fm_assert(t.insert(0, tracked{139}));
        fm_assert(t.insert(100, tracked{act(act_insert, 5)}));
        fm_assert(t.insert(last, tracked{149}));
        fm_assert(!present(t, 5));
        const uint64_t keys[] = { 0, 100, last };
        check_layout(t, 2 << 20, keys);
        fm_assert(t.erase(0).v == 139);
        fm_assert(!t.erase(0));
        fm_assert(tracked::live == 2);
        t.clear();
        fm_assert(tracked::live == 0);
        fm_assert(!present(t, 5) && !present(t, 100) && !present(t, last));
        check_layout(t, 2 << 20, {});
        fm_assert(t.insert(5, tracked{151}));
    }
    fm_assert(tracked::live == 0);
}

void check_one_split()
{
    constexpr auto key = [](uint64_t z, uint64_t p) { return z << 4 | p; };
    fm_assert(tracked::live == 0);
    {
        one_split_table t;
        set_target(t);
        fm_assert(!t.find(key(1, 0)));
        fm_assert(t.insert(key(0, 3), tracked{157}));
        fm_assert(!t.find(key(1, 0)));
        fm_assert(t.insert(key(1, 15), tracked{163}));
        fm_assert(!t.find(key(2, 0)));
        fm_assert(t.insert(key(2, 0), tracked{167}));
        fm_assert(t.page_count() == 3);
        const uint64_t keys[] = { key(0, 3), key(1, 15), key(2, 0) };
        check_layout(t, 16, keys);
        fm_assert(t.erase(key(0, 3)).v == 157);
        fm_assert(!t.erase(key(3, 1)));
        fm_assert(!t.erase(key(1, 14)));
        fm_assert(t.page_count() == 3);

        // destructors insert into a new side array page, and into a page clear() has passed
        // a value clear() misses dies in free_all_pages(), seen only by this hook
        tracked::on_destroy = [](uint64_t) { (void)check_layers(*target<one_split_table>, [](uint64_t, const tracked&) {}); };
        fm_assert(t.insert(key(1, 1), tracked{act(act_insert, key(5, 2))}));
        fm_assert(t.insert(key(2, 5), tracked{act(act_insert, key(0, 2))}));
        t.clear();
        fm_assert(tracked::live == 0);
        fm_assert(t.page_count() == 0);
        fm_assert(!t.raw_top()[0].side);
        check_layout(t, 16, {});
        t.clear();
        fm_assert(t.insert(key(5, 2), tracked{173}));
    }
    tracked::on_destroy = nullptr;
    fm_assert(tracked::live == 0);
}

void check_side_free()
{
    constexpr auto key = [](uint64_t top, uint64_t z, uint64_t p) { return top << 6 | z << 4 | p; };
    fm_assert(tracked::live == 0);
    {
        split_free_table t;
        set_target(t);
        const auto& e = t.raw_top()[5];
        fm_assert(t.insert(key(5, 1, 3), tracked{251}));
        fm_assert(t.insert(key(5, 2, 4), tracked{257}));
        fm_assert(t.insert(key(5, 2, 5), tracked{263}));
        fm_assert(t.page_count() == 2 && e.side && e.side[0].live == 2);
        const uint64_t keys[] = { key(5, 1, 3), key(5, 2, 4), key(5, 2, 5) };
        check_layout(t, size_t{24} << 3, keys);

        fm_assert(t.erase(key(5, 1, 3)).v == 251);
        fm_assert(t.page_count() == 1 && e.side && e.side[0].live == 1 && !e.side[1].page);
        fm_assert(t.erase(key(5, 2, 4)).v == 257);
        fm_assert(t.page_count() == 1 && e.side && e.side[0].live == 1);
        fm_assert(t.erase(key(5, 2, 5)).v == 263);
        fm_assert(t.page_count() == 0 && !e.side && t.raw_spare().page);
        fm_assert(!t.find(key(5, 2, 5)) && !t.find(key(5, 1, 3)));
        check_layout(t, size_t{24} << 3, {});

        fm_assert(t.insert(key(5, 0, 1), tracked{269}));
        fm_assert(t.page_count() == 1 && !e.side);

        // the erase frees the side array before the destructor allocates a new one
        fm_assert(t.insert(key(5, 3, 0), tracked{act(act_insert, key(5, 1, 0))}));
        fm_assert(e.side && e.side[0].live == 1);
        (void)t.erase(key(5, 3, 0));
        fm_assert(e.side && e.side[0].live == 1 && !e.side[3].page && e.side[1].page);
        fm_assert(t.find(key(5, 1, 0))->v == key(5, 1, 0) + 1);
        const uint64_t keys2[] = { key(5, 0, 1), key(5, 1, 0) };
        check_layout(t, size_t{24} << 3, keys2);

        // the destructor erases the other side page's last value during clear()
        fm_assert(t.insert(key(5, 2, 9), tracked{act(act_erase, key(5, 1, 0))}));
        fm_assert(e.side[0].live == 2);
        t.clear();
        fm_assert(tracked::live == 0 && t.page_count() == 0 && t.size() == 0 && !e.side);
        check_layout(t, size_t{24} << 3, {});
    }
    fm_assert(tracked::live == 0);
    tracked::hook = nullptr;
}

void check_one_entry()
{
    one_entry_table t;
    fm_assert(t.insert(0, 179));
    fm_assert(t.insert(31, 181));
    const uint64_t keys[] = { 0, 31 };
    check_layout(t, 16, keys);
    const uint64_t* page = t.raw_pages()[0].page;
    fm_assert(t.erase(0) == 179);
    fm_assert(t.erase(31) == 181);
    fm_assert(t.page_count() == 0 && t.raw_spare().page == page);
    check_layout(t, 16, {});
    fm_assert(t.insert(7, 191));
    fm_assert(t.raw_pages()[0].page == page && !t.raw_spare().page);
}

void check_wide()
{
    static_assert(wide_table::key_bits == 64);
    constexpr uint64_t keys[] = { 0, (uint64_t{1} << 43) - 1, uint64_t{1} << 43, uint64_t{1} << 63, ~uint64_t{0}, };
    check_ints<uint8_t, wide_params>(size_t{16} << 21, keys);
}

void check_perverse()
{
    constexpr uint64_t page = uint64_t{1} << id_like_table::page_bits;
    fm_assert(tracked::live == 0);
    {
        id_like_table t;
        set_target(t);
        // destructors insert during clear(): into the page being cleared, and into a new one
        fm_assert(t.insert(1025, tracked{act(act_insert, 1026)}));
        fm_assert(t.insert(3 * page + 1, tracked{act(act_insert, 7 * page)}));
        fm_assert(t.insert(3 * page + 2, tracked{193}));
        t.clear();
        fm_assert(tracked::live == 0 && t.page_count() == 0 && t.size() == 0);
        check_layout(t, 2 << 20, {});

        // a destructor clears its own table after erase()
        fm_assert(t.insert(1025, tracked{act(act_clear, 0)}));
        fm_assert(t.insert(page + 5, tracked{199}));
        (void)t.erase(1025);
        fm_assert(tracked::live == 0 && t.page_count() == 0 && t.size() == 0);
        check_layout(t, 2 << 20, {});
    }
    {
        id_like_table t;
        set_target(t);
        // the destructor re-inserts after the erase that freed its page, taking the spare
        constexpr uint64_t k = 5 * page + 3;
        fm_assert(t.insert(k, tracked{act(act_insert, k)}));
        const tracked* base = t.find(k) - (k & (page - 1));
        (void)t.erase(k);
        fm_assert(t.find(k) && t.find(k)->v == k + 1);
        fm_assert(t.find(k) - (k & (page - 1)) == base);
        fm_assert(t.page_count() == 1 && !t.raw_spare().page);
        const uint64_t keys[] = { k };
        check_layout(t, 2 << 20, keys);

        fm_assert(!t.erase(k + 1));
        fm_assert(t.size() == 1);
        check_layout(t, 2 << 20, keys);
    }
    {
        id_like_table a, b;
        set_target(a);
        // move-assign destroys the destination's values, whose destructors re-enter it
        fm_assert(a.insert(1025, tracked{act(act_erase, 1026)}));
        fm_assert(a.insert(1026, tracked{211}));
        fm_assert(a.insert(page + 1, tracked{act(act_insert, 2 * page)}));
        fm_assert(b.insert(3 * page, tracked{223}));
        a = move(b);
        fm_assert(tracked::live == 1);
        fm_assert(a.find(3 * page)->v == 223 && !a.find(1025) && !a.find(2 * page));
        fm_assert(a.page_count() == 1 && a.size() == 1);
        fm_assert(b.page_count() == 0 && b.raw_top().isEmpty());
        b = move(a);
        fm_assert(b.find(3 * page)->v == 223);
        fm_assert(a.raw_top().isEmpty());
    }
    fm_assert(tracked::live == 0);
    {
        id_like_table t;
        set_target(t);
        // the table's destructor runs a value destructor that inserts
        fm_assert(t.insert(1025, tracked{act(act_insert, 9 * page)}));
    }
    fm_assert(tracked::live == 0);
    tracked::hook = nullptr;
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

uint32_t below(xoshiro256starstar& rng, uint32_t n)
{
    return uint32_t((rng() >> 32) * n >> 32);
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
    xoshiro256starstar rng;
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
            const uint64_t b = i == 2 ? uint64_t{1} << shift : (rng() & key_mask) >> shift << shift;
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
        return rng() & key_mask;
    }

    uint64_t pick_key(bool ins)
    {
        if (p.mode == key_mode::walk)
        {
            const uint32_t n = p.windows * p.window_size;
            const uint64_t r = rng();
            if (r % 64 == 0)
                walk_pos = uint32_t((r >> 32) * n >> 32);
            else
                walk_pos = (walk_pos + n - 8 + uint32_t(r >> 8) % 17) % n;
            return window_key(walk_pos);
        }
        if constexpr (Table::key_bits > 16)
            if (!keys.isEmpty() && rng() % 8 < (ins ? 1u : 6u))
                return present_key();
        return fresh_key();
    }

    uint64_t make_value(uint64_t k)
    {
        if constexpr (is_tracked)
            if (p.hooks && rng() % 8 == 0)
            {
                const uint64_t r = rng();
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
                absent(rng() & key_mask);
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
        return rng() % 5 < insert_fifths && keys.size() < p.live_cap;
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
            s[d] = rng() & low_mask(site_bits[d]);
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
            at = { rng() & low_mask(ant_x_bits), rng() & low_mask(ant_y_bits), at[2] };
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
            const double u = double((rng() >> 11) + 1) * 0x1p-53;
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
            touch(key_at(at), rng() % 5 < insert_fifths && keys.size() < p.live_cap);
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
            if (first && dir == 1 && (rng() & 1))
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
            bool minus = (rng() & 1) != 0;
            if (o != 0 && o != m && below(rng, 4) != 0)
                minus = o <= m - o;
            at = moved(at, d * 2 + minus);
            break;
        }
        }
        touch(key_at(at), phase_insert(op));
    }

public:
    explicit random_run(const random_params& p) : p{p}, rng{p.seed} {}

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

template<typename Table>
void check_fill_block(uint64_t block, xoshiro256starstar& rng)
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
    xoshiro256starstar rng{seed};
    check_fill_block<Table>(0, rng);
    if constexpr (last_block > 0)
    {
        check_fill_block<Table>(last_block, rng);
        check_fill_block<Table>(rng() & last_block, rng);
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
        xoshiro256starstar rng{seed};
        for (uint32_t i = 0; i < 4096; i++)
            check_key(rng() & key_mask);
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

void check_plane_wide()
{
    using Table = plane_wide_table;
    constexpr uint32_t xs[] = { 0, 1, 0xffff, 0x10000, 0x1ffff, 0xffffffff, };
    Table t;
    uint64_t keys[12];
    uint32_t n = 0;
    for (uint32_t y = 0; y < 2; y++)
        for (uint32_t x : xs)
        {
            fm_assert(t.insert(x, y, uint8_t(n + 1)));
            keys[n++] = Table::pack(x, y);
        }
    fm_assert(t.page_count() == 3);
    check_layout(t, size_t{8} << 16, keys);
    n = 0;
    for (uint32_t y = 0; y < 2; y++)
        for (uint32_t x : xs)
        {
            fm_assert(t.find(x, y) == t.find(keys[n]) && *t.find(x, y) == n + 1);
            fm_assert(!t.insert(x, y, 1));
            n++;
        }
    fm_assert(t.find(2, 1) && !*t.find(2, 1) && t.find(0xfffffffe, 0) && !*t.find(0xfffffffe, 0));
    fm_assert(!t.find(0x20000, 0) && !t.find(0xfffeffff, 1));
    n = 0;
    for (uint32_t y = 0; y < 2; y++)
        for (uint32_t x : xs)
            fm_assert(t.erase(x, y) == ++n);
    fm_assert(t.page_count() == 3);
    check_layout(t, size_t{8} << 16, {});
}

void check_spare_paths()
{
    constexpr uint64_t page = uint64_t{1} << id_like_table::page_bits;
    fm_assert(tracked::live == 0);
    {
        id_like_table t;
        set_target(t);
        constexpr uint64_t k = 2 * page + 9;
        fm_assert(t.insert(k, tracked{227}));
        const tracked* base = t.find(k) - 9;
        // the value of a failed insert erases the occupant, whose page becomes the spare
        fm_assert(!t.insert(k, tracked{act(act_erase, k)}));
        fm_assert(!t.find(k) && t.page_count() == 0 && t.raw_spare().page == base);
        fm_assert(tracked::live == 0);

        // the value erases its own key after its page is gone
        fm_assert(t.insert(k, tracked{act(act_erase, k)}));
        fm_assert(!t.raw_spare().page);
        (void)t.erase(k);
        fm_assert(!t.find(k) && t.page_count() == 0 && t.raw_spare().page == base);
        fm_assert(tracked::live == 0);

        t.clear();
        fm_assert(t.page_count() == 0 && t.raw_spare().page == base);

        // a destructor's insert takes the spare, and clear() returns it
        fm_assert(t.insert(1025, tracked{229}));
        fm_assert(t.insert(page + 1, tracked{act(act_insert, 4 * page)}));
        fm_assert(t.erase(1025).v == 229);
        fm_assert(t.page_count() == 1 && t.raw_spare().page == base);
        t.clear();
        fm_assert(t.page_count() == 0 && t.raw_spare().page == base);
        fm_assert(tracked::live == 0);
        check_layout(t, 2 << 20, {});

        const uint64_t keys[] = { 7, page + 7, 2 * page + 7 };
        fm_assert(t.insert(keys[0], tracked{233}));
        fm_assert(t.insert(keys[1], tracked{239}));
        fm_assert(t.insert(keys[2], tracked{241}));
        fm_assert(t.erase(keys[0]).v == 233);
        // remove_page() moved the last record into the hole
        fm_assert(t.raw_pages()[0].page == t.find(keys[2]) - 7);
        check_layout(t, 2 << 20, {keys + 1, 2});
        fm_assert(t.erase(keys[2]).v == 241);
        check_layout(t, 2 << 20, {keys + 1, 1});
        fm_assert(t.page_count() == 1 && t.find(keys[1])->v == 239);
    }
    fm_assert(tracked::live == 0);
    tracked::hook = nullptr;
}

struct slot_obj final : bptr_base {};

template<mlt_params P>
void check_bptr_slots()
{
    using Table = multi_level_table<bptr<slot_obj>, P>;
    constexpr uint64_t k = 0x165 & ((uint64_t{1} << Table::key_bits) - 1);
    const auto check_gone = [](const Table& t) {
        const bptr<slot_obj>* x = t.find(k);
        fm_assert(!x || !x->has_block());
        if constexpr (P.free_empty)
            fm_assert(t.size() == 0 && t.page_count() == 0);
    };

    {
        auto p = bptr<slot_obj>{InPlace};
        {
            Table t;
            fm_assert(t.insert(k, p));
            p.destroy();
        }
    }
    {
        auto p = bptr<slot_obj>{InPlace};
        Table t;
        fm_assert(t.insert(k, p));
        p.destroy();
        t.clear();
        check_gone(t);
    }
    {
        auto p = bptr<slot_obj>{InPlace};
        Table t;
        fm_assert(t.insert(k, p));
        p.destroy();
        fm_assert(t.erase(k).has_block());
        check_gone(t);
    }
    {
        auto p = bptr<slot_obj>{InPlace};
        Table t;
        fm_assert(t.insert(k, p));
        p.destroy();
        fm_assert(!t.insert(k, bptr<slot_obj>{InPlace}));
        fm_assert(check_layers(t, [](uint64_t, const bptr<slot_obj>&) {}) == 1);
        fm_assert(t.erase(k).has_block());
        check_gone(t);
    }
}

void check_chunk_table()
{
    const int16_t coords[] = { INT16_MIN, INT16_MAX, -257, -256, 255, 256, };
    const int8_t zs[] = { chunk_z_min, chunk_z_max, };
    const auto fake = [](uint32_t i) { return reinterpret_cast<chunk*>(uintptr_t{64} * (i + 1)); };

    detail::chunk_table t;
    uint32_t i = 0;
    for (int8_t z : zs)
        for (int16_t v : coords)
            t.update_slot({v, v, z}, fake(i++));
    i = 0;
    for (int8_t z : zs)
        for (int16_t v : coords)
        {
            fm_assert(t.chunk_at({v, v, z}) == fake(i++));
            fm_assert(!t.chunk_at({int16_t(v + 1), v, z}) && !t.chunk_at({int16_t(v - 1), v, z}));
            fm_assert(!t.chunk_at({v, int16_t(v + 1), z}) && !t.chunk_at({v, int16_t(v - 1), z}));
        }

    // chunk_coords_ addition wraps
    t.update_slot({INT16_MIN, INT16_MAX, chunk_z_max}, fake(99));
    bool found = false;
    for (chunk* c : t.neighbors({INT16_MAX, INT16_MAX, chunk_z_max}))
        if (c == fake(99))
            found = true;
    fm_assert(found);
    t.update_slot({INT16_MIN, INT16_MAX, chunk_z_max}, nullptr);

    for (int8_t z : zs)
        for (int16_t v : coords)
        {
            t.update_slot({v, v, z}, nullptr);
            fm_assert(!t.chunk_at({v, v, z}));
        }
}

void check_world_ids()
{
    constexpr object_id max_id = (object_id{1} << object_table::key_bits) - 1;
    const object_id ids[] = { 1025, (1 << 18) - 1, 1 << 18, };
    constexpr chunk_coords_ ch{0, 0, 0};

    world w;
    uint32_t i = 0;
    for (object_id id : ids)
        (void)w.make_object<light>(id, {ch, local_coords{i++}}, light_proto{});
    w.set_object_counter(max_id - 1);
    const auto id = w.make_id();
    fm_assert(id == max_id);
    (void)w.make_object<light>(id, {ch, local_coords{i++}}, light_proto{});

    for (object_id x : ids)
    {
        const auto o = w.find_object(x);
        fm_assert(o && o->id == x);
    }
    fm_assert(w.find_object(max_id)->id == max_id);
    w.chunk_table_prepare_frame();

    {
        const auto o = w.find_object(ids[1]);
        o->chunk().kill_object(*o, o->index());
    }
    fm_assert(!w.find_object(ids[1]));
    w.chunk_table_prepare_frame();
}

void check_world_random_ids()
{
    constexpr object_id max_id = (object_id{1} << object_table::key_bits) - 1;
    constexpr chunk_coords_ ch{0, 0, 0};
    constexpr uint32_t count = 64;

    xoshiro256starstar rng{17};
    const object_id cluster = 1025 + rng() % (max_id - 1024 - 4096);
    world w;
    object_id ids[count];
    bool dead[count] = {};
    gtl::flat_hash_set<object_id> seen;
    for (uint32_t i = 0; i < count; )
    {
        const object_id id = i % 2 ? 1025 + rng() % (max_id - 1024) : cluster + rng() % 4096;
        if (!seen.insert(id).second)
            continue;
        ids[i] = id;
        (void)w.make_object<light>(id, {ch, local_coords{i}}, light_proto{});
        i++;
    }
    for (uint32_t i = 0; i < count; i++)
        if (rng() % 2)
        {
            const auto o = w.find_object(ids[i]);
            o->chunk().kill_object(*o, o->index());
            dead[i] = true;
        }
    w.chunk_table_prepare_frame();
    for (uint32_t i = 0; i < count; i++)
    {
        const auto o = w.find_object(ids[i]);
        if (dead[i])
            fm_assert(!o);
        else
            fm_assert(o && o->id == ids[i]);
    }
    for (uint32_t i = 0; i < 256; i++)
        if (const object_id id = 1 + rng() % max_id; !seen.contains(id))
            fm_assert(!w.find_object(id));
}

constexpr uint64_t pack_coords(chunk_coords_ c)
{
    return uint64_t{uint16_t(c.x)} | uint64_t{uint16_t(c.y)} << 16 | uint64_t{uint8_t(c.z)} << 32;
}

constexpr chunk_coords_ unpack_coords(uint64_t k)
{
    return { int16_t(uint16_t(k)), int16_t(uint16_t(k >> 16)), int8_t(uint8_t(k >> 32)) };
}

chunk* fake_chunk(uint32_t i)
{
    return reinterpret_cast<chunk*>(uintptr_t{64} * (i + 1));
}

// The key layout, derived apart from make_key() so the test doesn't compare it with itself.
constexpr uint32_t chunk_bias = (1u << 15) + (1u << 8);

constexpr int16_t chunk_coord(uint32_t outer, uint32_t side)
{
    return int16_t(uint16_t((outer << 9 | side) - chunk_bias));
}

constexpr uint32_t chunk_top_index(uint32_t oy, uint32_t ox, int8_t z)
{
    return oy << 11 | ox << 4 | uint32_t(z - chunk_z_min);
}

constexpr uint32_t chunk_slot(uint32_t ly, uint32_t lx)
{
    return ly << 9 | lx;
}

void check_chunk_table_layout()
{
    detail::chunk_table t;
    const auto& mlt = t.raw_table();
    t.update_slot({0, 0, 0}, fake_chunk(0));
    fm_assert(mlt.page_count() == 1);
    fm_assert(mlt.raw_pages()[0].top_index == chunk_top_index(64, 64, 0));
    chunk* const* page = mlt.raw_pages()[0].page;
    fm_assert(page[chunk_slot(256, 256)] == fake_chunk(0));

    t.update_slot({-256, 0, 0}, fake_chunk(1));
    t.update_slot({255, 0, 0}, fake_chunk(2));
    t.update_slot({0, -256, 0}, fake_chunk(3));
    t.update_slot({0, 255, 0}, fake_chunk(4));
    fm_assert(mlt.page_count() == 1);
    fm_assert(page[chunk_slot(256, 0)] == fake_chunk(1) && page[chunk_slot(256, 511)] == fake_chunk(2));
    fm_assert(page[chunk_slot(0, 256)] == fake_chunk(3) && page[chunk_slot(511, 256)] == fake_chunk(4));

    t.update_slot({-257, 0, 0}, fake_chunk(5));
    t.update_slot({256, 0, 0}, fake_chunk(6));
    t.update_slot({0, -257, 0}, fake_chunk(7));
    t.update_slot({0, 256, 0}, fake_chunk(8));
    t.update_slot({0, 0, 1}, fake_chunk(9));
    fm_assert(mlt.page_count() == 6);
    const uint32_t tops[] = {
        chunk_top_index(64, 64, 0), chunk_top_index(64, 63, 0), chunk_top_index(64, 65, 0),
        chunk_top_index(63, 64, 0), chunk_top_index(65, 64, 0), chunk_top_index(64, 64, 1),
    };
    for (uint32_t top : tops)
    {
        uint32_t found = 0;
        for (const auto& rec : mlt.raw_pages())
            found += rec.top_index == top;
        fm_assert(found == 1);
    }

    t = {};
    t.update_slot({INT16_MIN, 0, chunk_z_max}, fake_chunk(10));
    t.update_slot({INT16_MAX, 0, chunk_z_max}, fake_chunk(11));
    fm_assert(mlt.page_count() == 1);
    fm_assert(mlt.raw_pages()[0].top_index == chunk_top_index(64, 0, chunk_z_max));
    page = mlt.raw_pages()[0].page;
    fm_assert(page[chunk_slot(256, 256)] == fake_chunk(10) && page[chunk_slot(256, 255)] == fake_chunk(11));
}

void check_chunk_table_random()
{
    xoshiro256starstar rng{18};
    gtl::flat_hash_map<uint64_t, chunk*> ref;
    detail::chunk_table t;
    const auto& mlt = t.raw_table();
    uint32_t serial = 0;

    const auto lookup = [&](chunk_coords_ c) -> chunk* {
        const auto it = ref.find(pack_coords(c));
        return it != ref.end() ? it->second : nullptr;
    };
    const auto random_z = [&] { return int8_t(chunk_z_min + int32_t(rng() % uint32_t(chunk_z_count))); };
    const auto random_coords = [&] {
        const uint64_t r = rng();
        return chunk_coords_{int16_t(r), int16_t(r >> 16), random_z()};
    };
    const auto near_corner = [&] {
        const uint64_t r = rng();
        const auto x = int16_t(chunk_coord(uint32_t(r & 127), 0) - 8 + int32_t(r >> 8 & 15));
        const auto y = int16_t(chunk_coord(uint32_t(r >> 12 & 127), 0) - 8 + int32_t(r >> 20 & 15));
        return chunk_coords_{x, y, random_z()};
    };
    const auto check_neighbors = [&](chunk_coords_ c) {
        const auto nb = t.neighbors(c);
        for (uint32_t i = 0; i < 8; i++)
            fm_assert(nb[i] == lookup(c + world::neighbor_offsets[i]));
    };
    const auto check = [&] {
        uint32_t n = 0;
        for (const auto& rec : mlt.raw_pages())
        {
            fm_assert(!rec.zero_index);
            const uint32_t oy = rec.top_index >> 11, ox = rec.top_index >> 4 & 127;
            const auto z = int8_t(int32_t(rec.top_index & 15) + chunk_z_min);
            for (uint32_t i = 0; i < 1u << 18; i++)
                if (chunk* c = rec.page[i])
                {
                    fm_assert(lookup({chunk_coord(ox, i & 511), chunk_coord(oy, i >> 9), z}) == c);
                    n++;
                }
        }
        fm_assert(n == ref.size());
        for (const auto& [k, c] : ref)
        {
            fm_assert(t.chunk_at(unpack_coords(k)) == c);
            check_neighbors(unpack_coords(k));
        }
        for (uint32_t i = 0; i < 64; i++)
            check_neighbors(i % 2 ? random_coords() : near_corner());
    };

    chunk_coords_ pos{0, 0, 0};
    uint32_t resets = 0;
    for (uint32_t op = 1; op <= 8000; op++)
    {
        const uint64_t r = rng();
        if (op / 1000 % 4 == 3)
            pos = random_coords();
        else if (r % 256 == 0)
            pos = r >> 8 & 1 ? near_corner() : random_coords();
        else
        {
            pos = pos + world::neighbor_offsets[r >> 9 & 7];
            if ((r >> 12) % 32 == 0)
                pos.z = int8_t(chunk_z_min + (pos.z - chunk_z_min + (r >> 17 & 1 ? 1 : chunk_z_count - 1)) % chunk_z_count);
        }
        const bool ins = (r >> 20) % 5 < (op / 500 % 2 ? 1u : 4u);
        if (ins && !lookup(pos))
        {
            chunk* c = fake_chunk(serial++);
            t.update_slot(pos, c);
            ref[pack_coords(pos)] = c;
        }
        else if (!ins)
        {
            t.update_slot(pos, nullptr);
            ref.erase(pack_coords(pos));
        }
        fm_assert(t.chunk_at(pos) == lookup(pos));
        if (mlt.page_count() >= 8)
        {
            if (resets++ % 8 == 0)
                check();
            t = {};
            ref.clear();
        }
        if (op % 1000 == 0)
            check();
    }
    check();
}

} // namespace

void Test::test_multi_level_table()
{
#if fm_ASAN
    check_heap_array_bounds();
#endif
    check_chunk_like();
    check_id_like();
    check_inline_zero();
    check_flat();
    check_deep();
    check_ints<int8_t, byte_params>(2 << 20, edge_keys<multi_level_table<int8_t, byte_params>>::keys);
    check_ints<uint8_t, byte_params>(2 << 20, edge_keys<multi_level_table<uint8_t, byte_params>>::keys);
    check_ints<int16_t, short_params>(2 << 20, edge_keys<multi_level_table<int16_t, short_params>>::keys);
    check_ints<uint16_t, short_params>(2 << 20, edge_keys<multi_level_table<uint16_t, short_params>>::keys);
    check_ints<int64_t, id_like_params>(2 << 20, edge_keys<multi_level_table<int64_t, id_like_params>>::keys);
    check_ints<uint64_t, id_like_params>(2 << 20, edge_keys<multi_level_table<uint64_t, id_like_params>>::keys);
    check_wide();
    check_no_pages();
    check_one_split();
    check_side_free();
    check_one_entry();
    check_perverse();
    check_spare_paths();
    check_bptr_slots<split_free_params>();
    check_bptr_slots<object_table_params>();
    check_bptr_slots<split_params>();
    check_bptr_slots<flat_params>();
    check_plane_wide();

    check_pack<plane_table>(0);
    check_pack<plane_deep_free_table>(0);
    check_pack<plane_no_y_table>(0);
    check_pack<plane_zero_table>(0);
    check_pack<plane_zero_free_table>(0);
    check_pack<plane_no_pages_table>(0);
    check_pack<plane_wide_table>(89);
    check_pack<space_free_table>(0);
    check_pack<space_deep_table>(0);
    check_pack<space_zero_free_table>(0);
    check_pack<space_no_pages_table>(0);
    check_pack<space_one_entry_table>(0);
    check_pack<plane_large_table>(0);
    check_pack<space_large_table>(0);
    check_pack<chunk_table_mlt>(90);

    check_fill<chunk_like_table>(40);
    check_fill<id_like_table>(41);
    check_fill<split_table>(42);
    check_fill<flat_table>(43);
    check_fill<deep_table>(44);
    check_fill<deep_tracked_table>(45);
    check_fill<no_pages_table>(46);
    check_fill<one_split_table>(47);
    check_fill<one_entry_table>(48);
    check_fill<bit_page_table>(49);
    check_fill<bit_flat_table>(50);
    check_fill<bit_flat_int_table>(51);
    check_fill<bit_pair_table>(52);
    check_fill<bit_split_table>(53);
    check_fill<bit_triple_table>(54);
    check_fill<bit_deep_table>(55);
    check_fill<wide_top_table>(56);
    check_fill<narrow_top_table>(57);
    check_fill<bit_zero_table>(58);
    check_fill<wide_zero_table>(59);
    check_fill<zero_top_table>(60);
    check_fill<split_free_table>(61);
    check_fill<bit_split_free_table>(62);
    check_fill<plane_table>(63);
    check_fill<plane_deep_free_table>(64);
    check_fill<plane_no_y_table>(65);
    check_fill<plane_zero_table>(66);
    check_fill<plane_zero_free_table>(67);
    check_fill<plane_no_pages_table>(68);
    check_fill<plane_wide_table>(69);
    check_fill<space_free_table>(70);
    check_fill<space_deep_table>(71);
    check_fill<space_deep_int_table>(72);
    check_fill<space_zero_free_table>(73);
    check_fill<space_no_pages_table>(74);
    check_fill<space_one_entry_table>(75);
    check_fill<plane_large_table>(91);
    check_fill<space_large_table>(92);
    fm_assert(tracked::live == 0);

    constexpr auto walk = key_mode::walk;
    check_random<deep_table>({.seed = 1, .ops = 30000, .top_size_bytes = size_t{16} << 7});
    check_random<deep_tracked_table>({.seed = 2, .ops = 20000, .top_size_bytes = size_t{16} << 7, .hooks = true});
    check_random<deep_tracked_table>({.seed = 3, .ops = 20000, .top_size_bytes = size_t{16} << 7,
                                      .mode = walk, .windows = 6, .window_size = 16, .hooks = true});
    check_random<one_split_table>({.seed = 4, .ops = 20000, .top_size_bytes = 16, .hooks = true});
    check_random<split_table>({.seed = 5, .ops = 20000, .top_size_bytes = size_t{16} << 3});
    check_random<one_entry_table>({.seed = 6, .ops = 10000, .top_size_bytes = 16});
    check_random<flat_table>({.seed = 7, .ops = 10000, .top_size_bytes = size_t{2} << 6});
    check_random<no_pages_table>({.seed = 8, .ops = 8000, .top_size_bytes = 2 << 20, .hooks = true, .check_every = 4000});
    check_random<no_pages_table>({.seed = 9, .ops = 8000, .top_size_bytes = 2 << 20,
                                  .mode = walk, .windows = 6, .window_size = 512, .hooks = true, .check_every = 4000});
    check_random<id_like_table>({.seed = 10, .ops = 800, .top_size_bytes = 2 << 20, .hooks = true,
                                 .check_every = 400, .live_cap = 4});
    check_random<id_like_table>({.seed = 11, .ops = 12000, .top_size_bytes = 2 << 20,
                                 .mode = walk, .windows = 4, .window_size = 512, .hooks = true, .check_every = 6000});
    check_random<chunk_like_table>({.seed = 12, .ops = 2000, .top_size_bytes = 2 << 20, .check_every = 1000, .page_cap = 8});
    check_random<chunk_like_table>({.seed = 13, .ops = 12000, .top_size_bytes = 2 << 20,
                                    .mode = walk, .windows = 4, .window_size = 512, .check_every = 6000});
    check_random<wide_table>({.seed = 14, .ops = 12, .top_size_bytes = size_t{16} << 21, .check_every = 12, .page_cap = 2});
    check_random<wide_table>({.seed = 15, .ops = 200, .top_size_bytes = size_t{16} << 21,
                              .mode = walk, .windows = 4, .window_size = 64, .check_every = 100});

    constexpr random_params bits = { .ops = 8000, .hooks = true, .check_every = 512, .clear_every = 2000, .move_every = 1000 };
    const auto with = [&](uint64_t seed, size_t top_size_bytes) {
        random_params p = bits;
        p.seed = seed;
        p.top_size_bytes = top_size_bytes;
        return p;
    };
    check_random<bit_page_table>(with(20, 16));
    check_random<bit_flat_table>(with(21, 16));
    check_random<bit_flat_int_table>(with(22, 8));
    check_random<bit_pair_table>(with(23, 16));
    check_random<bit_split_table>(with(24, 16));
    check_random<bit_triple_table>(with(25, 32));
    check_random<bit_deep_table>(with(26, 128));
    check_random<wide_top_table>(with(27, 256));
    check_random<narrow_top_table>(with(28, 16));
    check_random<bit_zero_table>(with(29, 128));
    check_random<wide_zero_table>(with(30, 32));
    check_random<zero_top_table>(with(31, 16));
    check_random<bit_split_free_table>(with(32, 48));
    check_random<split_free_table>({.seed = 33, .ops = 20000, .top_size_bytes = size_t{24} << 3, .hooks = true});
    check_random<split_free_table>({.seed = 34, .ops = 20000, .top_size_bytes = size_t{24} << 3,
                                    .mode = walk, .windows = 6, .window_size = 8, .hooks = true});

    check_random<plane_table>(with(76, size_t{8} << 3));
    check_random<plane_deep_free_table>(with(77, size_t{16} << 5));
    check_random<plane_no_y_table>(with(78, size_t{8} << 3));
    check_random<plane_zero_table>(with(79, size_t{16} << 2));
    check_random<plane_zero_free_table>(with(80, size_t{24} << 2));
    check_random<plane_no_pages_table>(with(81, size_t{8} << 5));
    check_random<plane_wide_table>({.seed = 82, .ops = 200, .top_size_bytes = size_t{8} << 16, .check_every = 100, .page_cap = 8});
    check_random<space_free_table>(with(83, size_t{16} << 3));
    check_random<space_deep_table>({.seed = 84, .ops = 12000, .top_size_bytes = size_t{8} << 6, .hooks = true});
    check_random<space_deep_int_table>({.seed = 85, .ops = 12000, .top_size_bytes = size_t{8} << 6});
    check_random<space_zero_free_table>(with(86, size_t{24} << 2));
    check_random<space_no_pages_table>(with(87, size_t{8} << 4));
    check_random<space_one_entry_table>(with(88, 8));
    fm_assert(tracked::live == 0);

    {
        using enum walk_type;
        check_walk<deep_tracked_table>(100, simple);
        check_walk<plane_large_table>(101, simple);
        check_walk<space_large_table>(102, simple);
        check_walk<split_free_table>(103, levy_flight);
        check_walk<plane_deep_free_table>(104, levy_flight);
        check_walk<space_deep_table>(105, levy_flight);
        check_walk<one_split_table>(106, persistent);
        check_walk<plane_table>(107, persistent);
        check_walk<space_zero_free_table>(108, persistent);
        check_walk<split_free_table>(109, kinetic_growth);
        check_walk<plane_large_table>(110, kinetic_growth);
        check_walk<space_deep_table>(111, kinetic_growth);
        check_walk<split_table>(112, true_self_avoiding);
        check_walk<plane_zero_table>(113, true_self_avoiding);
        check_walk<space_deep_int_table>(114, true_self_avoiding);
        check_walk<deep_tracked_table>(115, loop_erased);
        check_walk<plane_deep_free_table>(116, loop_erased);
        check_walk<space_large_table>(117, loop_erased);
        check_walk<split_free_table>(118, vertex_reinforced);
        check_walk<plane_large_table>(119, vertex_reinforced);
        check_walk<space_zero_free_table>(120, vertex_reinforced);
        check_walk<one_split_table>(121, elephant);
        check_walk<plane_table>(122, elephant);
        check_walk<space_deep_table>(123, elephant);
        check_walk<split_table>(124, resetting);
        check_walk<plane_large_table>(125, resetting);
        check_walk<space_deep_int_table>(126, resetting);
        check_walk<deep_tracked_table>(127, random_environment);
        check_walk<plane_zero_table>(128, random_environment);
        check_walk<space_large_table>(129, random_environment);
        check_walk<split_free_table>(130, excited);
        check_walk<plane_deep_free_table>(131, excited);
        check_walk<space_deep_table>(132, excited);
        check_walk<deep_tracked_table>(133, langton_ant);
        check_walk<plane_large_table>(134, langton_ant, 12000);
        check_walk<space_deep_table>(135, langton_ant);
        check_walk<one_split_table>(136, rotor_router);
        check_walk<plane_table>(137, rotor_router);
        check_walk<space_zero_free_table>(138, rotor_router);
        check_walk<split_table>(139, internal_dla);
        check_walk<plane_large_table>(140, internal_dla);
        check_walk<space_large_table>(141, internal_dla);
        check_walk<deep_tracked_table>(142, eden);
        check_walk<plane_deep_free_table>(143, eden);
        check_walk<space_zero_free_table>(144, eden);
        check_retrace<split_free_table>(145, 1);
        check_retrace<plane_large_table>(146, 8);
        check_retrace<space_deep_table>(147, 8);
        check_walk<split_free_table>(148, page_corner_drift);
        check_walk<plane_deep_free_table>(149, page_corner_drift);
        check_walk<space_large_table>(150, page_corner_drift);
        check_retrace<split_free_table>(151, 4);
        check_retrace<plane_deep_free_table>(152, 4);
        check_retrace<space_large_table>(153, 8);
        check_retrace<deep_tracked_table>(154, 16);
        check_retrace<plane_large_table>(155, 16);
        check_retrace<plane_large_table>(156, 32);
        check_retrace<space_deep_table>(157, 32);
    }
    fm_assert(tracked::live == 0);

    check_chunk_table();
    check_chunk_table_layout();
    check_chunk_table_random();
    check_world_ids();
    check_world_random_ids();
}

} // namespace floormat
