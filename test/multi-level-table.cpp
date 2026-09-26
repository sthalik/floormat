#include "app.hpp"
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
#include <limits>
#include <type_traits>
#include <utility>

namespace floormat {

// Outside the anonymous namespace: GCC warns about members that an explicit
// instantiation with internal linkage never calls.
namespace mlt_test {

struct tracked
{
    static constexpr uint64_t act_flag = uint64_t{1} << 63;
    static inline uint32_t live = 0;
    static inline void (*hook)(uint32_t action, uint64_t key) = nullptr;

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
        if (x & act_flag)
            hook(uint32_t(x >> 61) & 3, x & ((uint64_t{1} << 61) - 1));
    }
};

} // namespace mlt_test

namespace {

using mlt_test::tracked;

enum : uint32_t { act_erase, act_insert, act_clear, };

static_assert(!std::is_copy_constructible_v<tracked> && !std::is_copy_assignable_v<tracked>);

// a value whose destructor calls tracked::hook: bits 61..62 hold the action, the rest the key
constexpr uint64_t act(uint32_t action, uint64_t key)
{
    return tracked::act_flag | uint64_t{action} << 61 | key;
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

using chunk_like_table = multi_level_table<uint64_t, chunk_like_params>;
using id_like_table = multi_level_table<tracked, id_like_params>;
using split_table = multi_level_table<uint32_t, split_params>;
using flat_table = multi_level_table<uint16_t, flat_params>;
using deep_table = multi_level_table<uint64_t, deep_params>;
using no_pages_table = multi_level_table<tracked, no_pages_params>;
using one_split_table = multi_level_table<tracked, one_split_params>;
using one_entry_table = multi_level_table<uint64_t, one_entry_params>;
using wide_table = multi_level_table<uint8_t, wide_params>;
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

} // namespace

template class multi_level_table<uint64_t, chunk_like_params>;
template class multi_level_table<tracked, id_like_params>;
template class multi_level_table<uint32_t, split_params>;
template class multi_level_table<uint16_t, flat_params>;
template class multi_level_table<uint64_t, deep_params>;
template class multi_level_table<int8_t, byte_params>;
template class multi_level_table<uint8_t, byte_params>;
template class multi_level_table<int16_t, short_params>;
template class multi_level_table<uint16_t, short_params>;
template class multi_level_table<int64_t, id_like_params>;
template class multi_level_table<uint64_t, id_like_params>;
template class multi_level_table<tracked, no_pages_params>;
template class multi_level_table<tracked, one_split_params>;
template class multi_level_table<uint64_t, one_entry_params>;
template class multi_level_table<uint8_t, wide_params>;

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

template<typename T, mlt_params P>
void check_layout(const multi_level_table<T, P>& t, size_t top_size_bytes, ArrayView<const uint64_t> keys)
{
    using Table = multi_level_table<T, P>;
    const auto top = t.raw_top();
    fm_assert(top.size() == size_t{1} << Table::top_bits);
    fm_assert(sizeof(top[0]) << Table::top_bits == top_size_bytes);
    if constexpr (P.top_source == mlt_source::superpage)
        fm_assert(t.raw_top_alloc().ptr == top.data() && t.raw_top_alloc().size == top_size_bytes);
    else
        fm_assert(!t.raw_top_alloc().ptr);

    if constexpr (!Table::has_pages)
    {
        for (uint64_t k : keys)
            fm_assert(t.find(k) == &top[k] && top[k]);
    }
    else
    {
        constexpr uint32_t page_bits = Table::page_bits, zero_bits = Table::zero_bits;
        constexpr size_t page_size = size_t{1} << page_bits;
        constexpr uint64_t page_mask = page_size - 1, zero_mask = (uint64_t{1} << zero_bits) - 1;

        const auto ref_at = [&](uint64_t ti, uint64_t zi) -> const auto& {
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
        };

        const auto pages = t.raw_pages();
        uint64_t total = 0;
        for (uint32_t i = 0; i < pages.size(); i++)
        {
            const auto& rec = pages[i];
            fm_assert(rec.page && rec.page != t.raw_spare().page);
            fm_assert(detail_mlt::page_of(ref_at(rec.top_index, rec.zero_index)) == rec.page);
            for (uint32_t j = 0; j < i; j++)
                fm_assert(pages[j].page != rec.page);
            if constexpr (P.free_empty)
            {
                uint32_t n = 0;
                for (size_t k = 0; k < page_size; k++)
                    if (rec.page[k])
                        n++;
                fm_assert(n == ref_at(rec.top_index, rec.zero_index).live);
                total += n;
            }
        }
        if constexpr (P.free_empty)
            fm_assert(total == t.size());

        // with the checks above, every page in the top is in the directory once
        size_t num_refs = 0;
        for (const auto& e : top)
        {
            if constexpr (zero_bits > 0)
            {
                if (detail_mlt::page_of(e.zero))
                    num_refs++;
                if (e.side)
                {
                    fm_assert(!detail_mlt::page_of(e.side[0]));
                    for (size_t z = 1; z <= zero_mask; z++)
                        if (detail_mlt::page_of(e.side[z]))
                            num_refs++;
                }
            }
            else if (detail_mlt::page_of(e))
                num_refs++;
        }
        fm_assert(num_refs == pages.size());

        if (const T* spare = t.raw_spare().page)
            for (size_t k = 0; k < page_size; k++)
                fm_assert(!spare[k]);

        for (uint64_t k : keys)
        {
            const T* s = t.find(k);
            fm_assert(s && *s);
            const T* page = detail_mlt::page_of(ref_at(k >> (page_bits + zero_bits), (k >> page_bits) & zero_mask));
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

        // the destructor inserts into the side array that clear() is freeing
        fm_assert(t.insert(key(1, 1), tracked{act(act_insert, key(5, 2))}));
        t.clear();
        fm_assert(tracked::live == 0);
        fm_assert(t.page_count() == 0);
        fm_assert(!t.raw_top()[0].side);
        check_layout(t, 16, {});
        t.clear();
        fm_assert(t.insert(key(5, 2), tracked{173}));
    }
    fm_assert(tracked::live == 0);
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

        // a destructor clears its own table from within clear(), then after erase()
        fm_assert(t.insert(1025, tracked{act(act_clear, 0)}));
        fm_assert(t.insert(page + 5, tracked{197}));
        t.clear();
        fm_assert(tracked::live == 0 && t.page_count() == 0);
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

template<typename Table>
void check_random(uint32_t num_keys, size_t top_size_bytes, uint64_t seed)
{
    using T = std::remove_cvref_t<decltype(*std::declval<const Table&>().find(0))>;
    constexpr auto value_of = [](const T& x) -> uint64_t {
        if constexpr (std::is_same_v<T, tracked>)
            return x.v;
        else
            return x;
    };

    xoshiro256starstar rng{seed};
    Array<uint64_t> ref{ValueInit, num_keys};
    Array<uint64_t> keys;
    arrayReserve(keys, num_keys);
    uint32_t num_present = 0;
    uint64_t next_value = 1;
    Table t;

    for (uint32_t op = 1; op <= 100000; op++)
    {
        const uint64_t r = rng();
        const auto k = uint32_t((r >> 32) * num_keys >> 32);
        const uint32_t insert_fifths = (op / 2000) % 2 ? 1 : 4;
        if (uint32_t(r) % 5 < insert_fifths)
        {
            const bool ok = t.insert(k, T(next_value));
            fm_assert(ok == !ref[k]);
            if (ok)
            {
                ref[k] = next_value;
                num_present++;
            }
            next_value++;
        }
        else
        {
            fm_assert(value_of(t.erase(k)) == ref[k]);
            if (ref[k])
                num_present--;
            ref[k] = 0;
        }
        if (const T* s = t.find(k); ref[k])
            fm_assert(s && value_of(*s) == ref[k]);
        else
            fm_assert(!s || !*s);

        if (op % 20000 == 0)
        {
            t.clear();
            for (uint64_t& x : ref)
                x = 0;
            num_present = 0;
        }
        else if (op % 5000 == 0)
        {
            Table tmp{move(t)};
            t = move(tmp);
        }
        if (op % 1024 == 0)
        {
            arrayClear(keys);
            for (uint32_t i = 0; i < num_keys; i++)
                if (ref[i])
                    arrayAppend(keys, uint64_t{i});
            fm_assert(keys.size() == num_present);
            if constexpr (std::is_same_v<T, tracked>)
                fm_assert(tracked::live == num_present);
            check_layout(t, top_size_bytes, keys);
        }
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

} // namespace

void Test::test_multi_level_table()
{
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
    check_one_entry();
    check_perverse();
    check_random<deep_table>(4096, size_t{16} << 7, 1);
    check_random<one_split_table>(128, 16, 2);
    fm_assert(tracked::live == 0);
    check_chunk_table();
    check_world_ids();
}

} // namespace floormat
