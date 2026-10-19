#include "app.hpp"
#include "multi-level-table-common.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/limits.hpp"
#include "src/object-table.hpp"

namespace floormat {

using namespace floormat::mlt_test;

namespace {

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
static_assert(valid_params<mlt_params{ .levels = { {.bits = {2, 1, 0}}, {.bits = {3, 1, 0}, .dynamic = true}, } }>);
static_assert(valid_params<mlt_params{ .levels = { {.bits = {16, 0, 0}}, {.bits = {16, 1, 0}, .dynamic = true}, } }>);
static_assert(valid_params<mlt_params{ .levels = { {.bits = {1, 0, 2}}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = {2, 1, 0}}, {.bits = 4, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = {17, 0, 0}}, {.bits = {16, 1, 0}, .dynamic = true}, } }>);
static_assert(!valid_params<mlt_params{ .levels = { {.bits = {0, 0, 0}}, {.bits = {3, 1, 0}, .dynamic = true}, } }>);

static_assert(move_only<chunk_like_table> && move_only<id_like_table> && move_only<split_table> &&
              move_only<flat_table> && move_only<deep_table> && move_only<no_pages_table> &&
              move_only<one_split_table> && move_only<one_entry_table> && move_only<wide_table>);
static_assert(move_only<object_table>);

static_assert(top_bytes<object_table> == 2 << 20 && sizeof(bptr<object>) << object_table::page_bits == 2 << 20);

template<typename Table, typename T>
T value_at(const Table& t, uint64_t key)
{
    const auto* s = t.find(key);
    fm_assert(s);
    return *s;
}

#if fm_ASAN
void check_heap_array_bounds()
{
    auto* p = new uint16_t[5]{};
    check_heap_array(p, 5);
    fm_assert(__asan_region_is_poisoned(p, 6 * sizeof(uint16_t)));
    fm_assert(!__asan_address_is_poisoned(p + 4));
    delete[] p;
}

#endif
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
    using L = limits<T>;
    constexpr auto values = [] {
        if constexpr (std::is_signed_v<T>)
            return std::array<T, 4>{L::min, L::max, T(-1), T(1)};
        else
            return std::array<T, 2>{L::max, T(1)};
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

    // An occupancy bug makes clear() and the destructor loop forever and erase() or insert() fail an assert.
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
        {
            Table t;
            fm_assert(t.insert(k, p));
            p.destroy();
        }
    }
}

} // namespace

uint32_t mlt_test::tracked::live = 0;
void (*mlt_test::tracked::hook)(uint32_t action, uint64_t key) = nullptr;
void (*mlt_test::tracked::on_destroy)(uint64_t v) = nullptr;

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
template class multi_level_table<tracked, split_free_params>;
template class multi_level_table<uint64_t, one_entry_params>;
template class multi_level_table<uint8_t, wide_params>;

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

    check_fill<chunk_like_table>(40);
    check_fill<id_like_table>(41);
    check_fill<split_table>(42);
    check_fill<flat_table>(43);
    check_fill<deep_table>(44);
    check_fill<deep_tracked_table>(45);
    check_fill<no_pages_table>(46);
    check_fill<one_split_table>(47);
    check_fill<one_entry_table>(48);
    check_fill<split_free_table>(61);
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
    check_random<split_free_table>({.seed = 33, .ops = 20000, .top_size_bytes = size_t{24} << 3, .hooks = true});
    check_random<split_free_table>({.seed = 34, .ops = 20000, .top_size_bytes = size_t{24} << 3,
                                    .mode = walk, .windows = 6, .window_size = 8, .hooks = true});
    fm_assert(tracked::live == 0);

    {
        using enum walk_type;
        check_walk<deep_tracked_table>(100, simple);
        check_walk<split_free_table>(103, levy_flight);
        check_walk<one_split_table>(106, persistent);
        check_walk<split_free_table>(109, kinetic_growth);
        check_walk<split_table>(112, true_self_avoiding);
        check_walk<deep_tracked_table>(115, loop_erased);
        check_walk<split_free_table>(118, vertex_reinforced);
        check_walk<one_split_table>(121, elephant);
        check_walk<split_table>(124, resetting);
        check_walk<deep_tracked_table>(127, random_environment);
        check_walk<split_free_table>(130, excited);
        check_walk<deep_tracked_table>(133, langton_ant);
        check_walk<one_split_table>(136, rotor_router);
        check_walk<split_table>(139, internal_dla);
        check_walk<deep_tracked_table>(142, eden);
        check_retrace<split_free_table>(145, 1);
        check_walk<split_free_table>(148, page_corner_drift);
        check_retrace<split_free_table>(151, 4);
        check_retrace<deep_tracked_table>(154, 16);
    }
    fm_assert(tracked::live == 0);

    mlt_test::test_bit_tables();
    mlt_test::test_plane_tables();
    mlt_test::test_space_tables();
}

} // namespace floormat
