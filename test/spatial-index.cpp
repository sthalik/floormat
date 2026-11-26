#include "app.hpp"
#include "src/spatial-index.hpp"
#include "src/spatial-index.inl"
#include "random/random.hpp"
#include <algorithm>
#include <bit>
#include <type_traits>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

i32 uniform(Random::xoshiro256starstar& r, i32 lo, i32 hi)
{
    return lo + (i32)(Random::next(r) % (u64)(hi - lo + 1));
}

template<typename T, u32 N>
struct spatial_index_tests
{
    using Index = spatial_index<T, N>;
    using Rect = Index::Rect;
    using block_type = Index::block_type;
    static constexpr u32 capacity = block_type::capacity;
    static constexpr i32 scale = sizeof(T) == 2 ? 1 : 100003;
    static constexpr u32 targets[] = { N + 5, 2, ~0u, N, N / 2, N + 1, 0, ~0u, 41, 3 * N + 9 };

    struct entry
    {
        Rect rect;
        u64 id;
    };

    struct state
    {
        Index index;
        Array<entry> live;
        Random::xoshiro256starstar rng;
        u64 next_id = 1;
        u32 max_entries_in_blocks = 0;
        bool is_using_tree = false;
    };

    static Rect random_rect(Random::xoshiro256starstar& r, i32 lo, i32 hi, i32 max_size)
    {
        const Vector2i min{uniform(r, lo, hi), uniform(r, lo, hi)};
        const Vector2i size{uniform(r, 0, max_size), uniform(r, 0, max_size)};
        return {Math::Vector2<T>{min * scale}, Math::Vector2<T>{(min + size) * scale}};
    }

    static Rect random_entry_rect(Random::xoshiro256starstar& r) { return random_rect(r, -300, 700, 96); }

    template<typename Compare, typename V>
    static bool is_hit(const Rect& e, const V& min, const V& max)
    {
        for (u32 axis = 0; axis < 2; axis++)
        {
            const double lo = (double)min[axis], hi = (double)max[axis];
            const double e_lo = (double)e.min()[axis], e_hi = (double)e.max()[axis];
            if constexpr(std::is_same_v<Compare, cmp::less<>>)
            {
                if (!(e_lo < hi && e_hi > lo))
                    return false;
            }
            else
            {
                if (!(e_lo <= hi && e_hi >= lo))
                    return false;
            }
        }
        return true;
    }

    static void sort_by_id(Array<entry>& a)
    {
        std::sort(a.begin(), a.end(), [](const entry& x, const entry& y) { return x.id < y.id; });
    }

    static void check_equal(const Array<entry>& a, const Array<entry>& b)
    {
        fm_assert_equal(a.size(), b.size());
        for (u32 i = 0; i < a.size(); i++)
        {
            fm_assert_equal(a[i].id, b[i].id);
            fm_assert(a[i].rect == b[i].rect);
        }
    }

    template<typename Compare, typename V>
    static void check_query(state& s, const V& min, const V& max)
    {
        Array<entry> expected;
        arrayReserve(expected, s.live.size());
        for (const auto& e : s.live)
            if (is_hit<Compare>(e.rect, min, max))
                arrayAppend(expected, e);

        Array<entry> found;
        arrayReserve(found, s.live.size());
        const auto num_found = s.index.template Search<Compare>(min, max, [&](u64 id, const Rect& r) {
            arrayAppend(found, entry{r, id});
            return true;
        });
        fm_assert_equal((usize)num_found, found.size());

        const auto k = (u32)uniform(s.rng, 1, 3);
        Array<entry> first;
        arrayReserve(first, k);
        const auto num_first = s.index.template Search<Compare>(min, max, [&](u64 id, const Rect& r) {
            arrayAppend(first, entry{r, id});
            return first.size() < k;
        });
        fm_assert_equal((usize)num_first, first.size());
        fm_assert_equal(Math::min((usize)k, found.size()), first.size());
        for (u32 i = 0; i < first.size(); i++)
        {
            fm_assert_equal(found[i].id, first[i].id);
            fm_assert(found[i].rect == first[i].rect);
        }

        if (s.is_using_tree)
        {
            sort_by_id(expected);
            sort_by_id(found);
        }
        check_equal(expected, found);
    }

    template<typename V>
    static void check_query_both_comparisons(state& s, const V& min, const V& max)
    {
        check_query<cmp::less_equal<>>(s, min, max);
        check_query<cmp::less<>>(s, min, max);
    }

    static void check_queries(state& s)
    {
        for (u32 q = 0; q < 4; q++)
        {
            const auto r = random_rect(s.rng, -400, 800, 320);
            const Vector2i min{r.min()}, max{r.max()};
            check_query_both_comparisons(s, min, max);

            const float fmin[2] = {(float)min.x() - (float)uniform(s.rng, 0, 2) * 0.5f * (float)scale,
                                   (float)min.y() + (float)uniform(s.rng, 0, 2) * 0.5f * (float)scale};
            const float fmax[2] = {(float)max.x() + (float)uniform(s.rng, 0, 2) * 0.5f * (float)scale,
                                   (float)max.y() - (float)uniform(s.rng, 0, 1) * 0.5f * (float)scale};
            if (fmin[0] <= fmax[0] && fmin[1] <= fmax[1])
                check_query_both_comparisons(s, fmin, fmax);
        }
        check_query_both_comparisons(s, Vector2i{-2000000000}, Vector2i{2000000000});
        const float fmin[2] = {-3e9f, -3e9f}, fmax[2] = {3e9f, 3e9f};
        check_query_both_comparisons(s, fmin, fmax);
    }

    static void check_state(const state& s)
    {
        fm_assert_equal((u32)s.live.size(), s.index.Count());
        fm_assert_equal(s.is_using_tree, s.index.is_using_tree());

        const auto blocks = s.index.raw_blocks();
        const auto num_blocks_needed = (s.max_entries_in_blocks + capacity - 1) / capacity;
        fm_assert_equal(Math::min(Index::max_blocks, std::bit_ceil(Math::max(1u, num_blocks_needed))),
                        (u32)blocks.size());
        fm_assert((uintptr_t)blocks.data() % 64 == 0);

        const auto num_in_blocks = s.is_using_tree ? 0u : (u32)s.live.size();
        for (u32 j = 0; j < blocks.size() * capacity; j++)
        {
            const auto& b = blocks[j / capacity];
            const auto i = j % capacity;
            const Rect r{{b.min_x[i], b.min_y[i]}, {b.max_x[i], b.max_y[i]}};
            if (j < num_in_blocks)
            {
                fm_assert(r == s.live[j].rect);
                fm_assert_equal(s.live[j].id, b.id[i]);
            }
            else
            {
                fm_assert(r == Index::tree_type::empty_rect);
                fm_assert_equal(u64{0}, b.id[i]);
            }
        }
    }

    static void insert_entry(state& s)
    {
        const entry e{random_entry_rect(s.rng), s.next_id++};
        s.index.Insert(e.rect, e.id);
        if (!s.is_using_tree && s.live.size() == N)
            s.is_using_tree = true;
        else if (!s.is_using_tree)
            s.max_entries_in_blocks = Math::max(s.max_entries_in_blocks, (u32)s.live.size() + 1);
        arrayAppend(s.live, e);
    }

    static void remove_entry(state& s)
    {
        const auto p = (u32)uniform(s.rng, 0, (i32)s.live.size() - 1);
        const auto e = s.live[p];
        auto rect = e.rect;
        if (uniform(s.rng, 0, 3) == 0)
            rect = {rect.min() - Math::Vector2<T>{(T)scale}, rect.max() + Math::Vector2<T>{(T)scale}};
        s.index.Remove(rect, e.id);
        s.live[p] = s.live.back();
        arrayRemoveSuffix(s.live, 1);
    }

    static void move_entry(state& s)
    {
        const auto p = (u32)uniform(s.rng, 0, (i32)s.live.size() - 1);
        const auto e = s.live[p];
        const entry e1{random_entry_rect(s.rng), uniform(s.rng, 0, 1) ? e.id : s.next_id++};
        fm_assert(s.index.Move(e.rect, e.id, e1.rect, e1.id));
        s.live[p] = e1;
    }

    static void check_missing_id(state& s)
    {
        const auto rect = random_entry_rect(s.rng);
        s.index.Remove(rect, s.next_id + 1000);
        fm_assert(!s.index.Move(rect, s.next_id + 1000, rect, s.next_id + 1001));
    }

    static void remove_all_entries(state& s)
    {
        s.index.RemoveAll();
        arrayClear(s.live);
        s.is_using_tree = false;
    }

    static void test_kernel(Random::xoshiro256starstar& r)
    {
        for (u32 iter = 0; iter < 2000; iter++)
        {
            block_type block;
            for (u32 i = 0; i < capacity; i++)
            {
                const auto rect = uniform(r, 0, 7) == 0 ? Index::tree_type::empty_rect : random_rect(r, 0, 160, 96);
                block.min_x[i] = rect.min().x();
                block.min_y[i] = rect.min().y();
                block.max_x[i] = rect.max().x();
                block.max_y[i] = rect.max().y();
                block.id[i] = i;
            }
            auto q = random_rect(r, -50, 200, 96);
            if (uniform(r, 0, 3) == 0)
            {
                const auto i = (u32)uniform(r, 0, capacity - 1);
                if (block.min_x[i] <= block.max_x[i])
                {
                    q.min().x() = block.max_x[i];
                    q.max().x() = Math::max(q.max().x(), q.min().x());
                }
            }
            u32 expected = 0;
            for (u32 i = 0; i < capacity; i++)
            {
                const Rect e{{block.min_x[i], block.min_y[i]}, {block.max_x[i], block.max_y[i]}};
                if (is_hit<cmp::less_equal<>>(e, q.min(), q.max()))
                    expected |= 1u << i;
            }
            fm_assert_equal(expected, Index::OverlappingEntries(block, q));
        }
    }

    static void test_random_sequence(u64 seed)
    {
        state s;
        Random::seed(s.rng, seed);
        arrayReserve(s.live, Math::max(41u, 3 * N + 9));
        for (const auto target : targets)
        {
            if (target == ~0u)
            {
                remove_all_entries(s);
                check_state(s);
                check_queries(s);
                continue;
            }
            for (u32 step = 0; s.live.size() != target || step < 24; step++)
            {
                const auto op = uniform(s.rng, 0, 9);
                if (s.live.size() < target && op < 7 || s.live.isEmpty())
                    insert_entry(s);
                else if (s.live.size() > target && op < 7)
                    remove_entry(s);
                else if (op < 9)
                    move_entry(s);
                else
                    check_missing_id(s);
                check_state(s);
                if (step % 4 == 0)
                    check_queries(s);
            }
        }
    }

    static void run()
    {
        Random::xoshiro256starstar r;
        Random::seed(r, 211 + N + sizeof(T));
        test_kernel(r);
        test_random_sequence(223 + N + sizeof(T));
        test_random_sequence(227 + N + sizeof(T));
    }
};

} // namespace

void Test::test_spatial_index()
{
    spatial_index_tests<i16, 0>::run();
    spatial_index_tests<i16, 8>::run();
    spatial_index_tests<i16, 128>::run();
    spatial_index_tests<i32, 0>::run();
    spatial_index_tests<i32, 8>::run();
    spatial_index_tests<i32, 128>::run();
}

} // namespace floormat
