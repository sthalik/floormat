#include "app.hpp"
#include "compat/multi-level-table.inl"
#include <utility>

namespace floormat {

namespace {

struct tracked
{
    static constexpr uint64_t reenter_flag = uint64_t{1} << 63;
    static inline uint32_t live = 0;

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
    void reset() noexcept;
};

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

using chunk_like_table = multi_level_table<uint64_t, chunk_like_params>;
using id_like_table = multi_level_table<tracked, id_like_params>;
using split_table = multi_level_table<uint32_t, split_params>;
using flat_table = multi_level_table<uint16_t, flat_params>;
using deep_table = multi_level_table<uint64_t, deep_params>;

id_like_table* reenter_table = nullptr;

void tracked::reset() noexcept
{
    if (!v)
        return;
    const auto x = std::exchange(v, 0);
    live--;
    if (x & reenter_flag)
    {
        const auto target = x & ~reenter_flag;
        (void)reenter_table->find(target);
        (void)reenter_table->erase(target);
    }
}

} // namespace

template class multi_level_table<uint64_t, chunk_like_params>;
template class multi_level_table<tracked, id_like_params>;
template class multi_level_table<uint32_t, split_params>;
template class multi_level_table<uint16_t, flat_params>;
template class multi_level_table<uint64_t, deep_params>;

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
        reenter_table = &t;
        fm_assert(t.insert(k0, tracked{31}));
        fm_assert(t.insert(k1, tracked{37}));
        fm_assert(t.insert(k2, tracked{41}));
        fm_assert(t.page_count() == 3);
        fm_assert(tracked::live == 3);
        fm_assert(!t.insert(k0, tracked{43}));
        fm_assert(tracked::live == 3);
        fm_assert(t.size() == 3);
        fm_assert(t.find(k0)->v == 31 && t.find(k1)->v == 37 && t.find(k2)->v == 41);

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

        fm_assert(t.insert(k3 + 1, tracked{(k3 + 2) | tracked::reenter_flag}));
        fm_assert(t.insert(k3 + 2, tracked{53}));
        fm_assert(t.erase(k3).v == 47);
        (void)t.erase(k3 + 1);
        fm_assert(!t.find(k3 + 2));
        fm_assert(t.page_count() == 2);
        fm_assert(tracked::live == 2);
        fm_assert(t.size() == 2);

        fm_assert(t.insert(k3, tracked{59 | tracked::reenter_flag}));
        fm_assert(t.insert(59, tracked{k3 | tracked::reenter_flag}));
        fm_assert(t.insert(k2 - 1, tracked{k0 | tracked::reenter_flag}));
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
        reenter_table = &t2;
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
        reenter_table = nullptr;
    }
    fm_assert(tracked::live == 0);
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
    fm_assert(t.erase(0) == 103);
    fm_assert(t.page_count() == 0);
    flat_table t2{move(t)};
    fm_assert(*t2.find(63) == 101);
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

} // namespace

void Test::test_multi_level_table()
{
    check_chunk_like();
    check_id_like();
    check_inline_zero();
    check_flat();
    check_deep();
}

} // namespace floormat
