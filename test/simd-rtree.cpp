#include "app.hpp"
#include "src/RTree.hpp"
#include "src/rtree-pool.inl"
#include "compat/limits.hpp"
#include "random/random.hpp"
#include <algorithm>
#include <bit>
#include <type_traits>
#include <utility>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

i32 uniform(Random::xoshiro256starstar& r, i32 lo, i32 hi)
{
    return lo + (i32)(Random::next(r) % (u64)(hi - lo + 1));
}

void check_same_ids(Array<u64>& found, const Array<u64>& expected, u32 num_found)
{
    fm_assert_equal((usize)num_found, found.size());
    std::sort(found.begin(), found.end());
    fm_assert_equal(expected.size(), found.size());
    for (u32 i = 0; i < found.size(); i++)
        fm_assert_equal(expected[i], found[i]);
}

constexpr u32 no_index = (u32)-1;

enum class tree_moves : u8 { none, any, out_of_place_only };

template<typename T>
struct rtree_tests
{
    using Tree = RTree<T, 32>;
    using Rect = Tree::Rect;
    using Node = Tree::Node;
    using Vec = Math::Vector2<T>;

    static constexpr i64 scale = sizeof(T) == 4 ? 1 << 14 : 1;
    static constexpr i64 search_max = sizeof(T) == 4 ? i64{1} << 30 : 32766;

    struct entry
    {
        Rect rect;
        u64 id;
    };

    struct tree_state
    {
        explicit tree_state(u32 capacity);

        Tree t;
        Array<entry> live;
        Array<u32> live_index_of_id{DirectInit, 16384, no_index};
        Array<bool> is_seen{ValueInit, 16384};
        Array<entry> hits, expected_hits;
        bool needs_minimum_bounding_rects = true;
        u32 num_removes_freeing_nodes = 0, num_moves_freeing_nodes = 0, num_root_level_drops = 0;
        u32 num_moves_in_place = 0, num_moves_out_of_place = 0, num_early_stops = 0;
    };

    struct branch_pos
    {
        const Node* node = nullptr;
        u32 index = 0;
    };

    static i64 map_coord(i64 x);
    static float map_float_coord(float x);
    static Vec map_point(Vector2i p);
    static Vector2i map_query_point(Vector2i p);
    static Vector2 map_query_point(Vector2 p);
    static Rect map_rect(const Range2Di& r);
    static Vector2i unmap_point(Vec p);
    static Rect random_rect(Random::xoshiro256starstar& r, i32 lo, i32 hi, i32 max_size);
    static Rect branch_rect(const Node& n, u32 i);
    static void set_branch_rect(Node& n, u32 i, const Rect& rect);
    static bool contains(const Rect& outer, const Rect& inner);
    static u32 overlapping_branches_ref(const Node& n, const Rect& q);
    static u32 containing_branches_ref(const Node& n, const Rect& q);
    template<typename Compare> static bool overlaps_float(const Rect& e, const float min[2], const float max[2]);
    template<typename Compare> static bool overlaps_int(const Rect& e, Vector2i min, Vector2i max);
    template<typename Compare>
    static void check_search(const Tree& t, const Array<entry>& es, const float min[2], const float max[2],
                             Array<u64>& found, Array<u64>& expected);
    template<typename Compare>
    static void check_search(const Tree& t, const Array<entry>& es, Vector2i min, Vector2i max,
                             Array<u64>& found, Array<u64>& expected);
    template<u32 N> static Node make_leaf(const Range2Di (&rects)[N]);
    static Rect minimum_bounding_rect(const Node& n);
    static bool has_minimum_bounding_rects(const Node& n);
    static void check_tree(const Node& n, bool is_root = true);
    static u32 check_leaf_entries(const Node& n, tree_state& s);
    static void check_state(tree_state& s);
    static branch_pos find_entry(const Node& n, u64 id);
    static branch_pos find_branch_to_leaf(const Node& n, u64 id);
    static bool is_move_in_place(const Node& root, u64 id, const Rect& rect);
    static void insert_entry(tree_state& s, const entry& e);
    static void remove_entry(tree_state& s, u32 index, const Rect& rect);
    static void remove_entry(tree_state& s, u32 index);
    static void move_entry(tree_state& s, u32 index, const Rect& rect, u64 id);
    static void remove_all_entries(tree_state& s);
    static bool is_id_less(const entry& a, const entry& b);
    template<typename Compare> static void check_hits_with(tree_state& s, const float min[2], const float max[2]);
    static void check_hits(tree_state& s, const float min[2], const float max[2]);
    static void check_same(const Node& a, const Node& b);
    static void check_copies(tree_state& s);

    static void test_kernels();
    static void test_search();
    static void test_search_bounds();
    static void test_pick_branch();
    static void test_split_of_two_clusters();
    static void test_random_sequence(u64 seed, tree_moves moves);
    static void test_critter_sequence();
    static void test_lattice_sequence();
    static void run();
};

template<typename T>
rtree_tests<T>::tree_state::tree_state(u32 capacity)
{
    arrayReserve(live, capacity);
    arrayReserve(hits, capacity);
    arrayReserve(expected_hits, capacity);
}

template<typename T>
i64 rtree_tests<T>::map_coord(i64 x)
{
    return x * scale;
}

template<typename T>
float rtree_tests<T>::map_float_coord(float x)
{
    return (float)((double)x * scale);
}

template<typename T>
auto rtree_tests<T>::map_point(Vector2i p) -> Vec
{
    return {(T)map_coord(p.x()), (T)map_coord(p.y())};
}

template<typename T>
Vector2i rtree_tests<T>::map_query_point(Vector2i p)
{
    return {(i32)map_coord(p.x()), (i32)map_coord(p.y())};
}

template<typename T>
Vector2 rtree_tests<T>::map_query_point(Vector2 p)
{
    return {map_float_coord(p.x()), map_float_coord(p.y())};
}

template<typename T>
auto rtree_tests<T>::map_rect(const Range2Di& r) -> Rect
{
    return {map_point(r.min()), map_point(r.max())};
}

template<typename T>
Vector2i rtree_tests<T>::unmap_point(Vec p)
{
    return {(i32)(p.x() / scale), (i32)(p.y() / scale)};
}

template<typename T>
auto rtree_tests<T>::random_rect(Random::xoshiro256starstar& r, i32 lo, i32 hi, i32 max_size) -> Rect
{
    const Vector2i min{uniform(r, lo, hi), uniform(r, lo, hi)};
    const Vector2i size{uniform(r, 0, max_size), uniform(r, 0, max_size)};
    return map_rect({min, min + size});
}

template<typename T>
auto rtree_tests<T>::branch_rect(const Node& n, u32 i) -> Rect
{
    return {{n.min_x[i], n.min_y[i]}, {n.max_x[i], n.max_y[i]}};
}

template<typename T>
void rtree_tests<T>::set_branch_rect(Node& n, u32 i, const Rect& rect)
{
    n.min_x[i] = rect.min().x();
    n.min_y[i] = rect.min().y();
    n.max_x[i] = rect.max().x();
    n.max_y[i] = rect.max().y();
}

template<typename T>
bool rtree_tests<T>::contains(const Rect& outer, const Rect& inner)
{
    return (outer.min() <= inner.min()).all() && (inner.max() <= outer.max()).all();
}

template<typename T>
u32 rtree_tests<T>::overlapping_branches_ref(const Node& n, const Rect& q)
{
    u32 bits = 0;
    for (u32 i = 0; i < 32; i++)
        if (n.min_x[i] <= q.max().x() && n.min_y[i] <= q.max().y() && n.max_x[i] >= q.min().x() &&
            n.max_y[i] >= q.min().y())
            bits |= 1u << i;
    return bits;
}

template<typename T>
u32 rtree_tests<T>::containing_branches_ref(const Node& n, const Rect& q)
{
    u32 bits = 0;
    for (u32 i = 0; i < 32; i++)
        if (n.min_x[i] <= q.min().x() && n.min_y[i] <= q.min().y() && n.max_x[i] >= q.max().x() &&
            n.max_y[i] >= q.max().y())
            bits |= 1u << i;
    return bits;
}

template<typename T>
void rtree_tests<T>::test_kernels()
{
    Random::xoshiro256starstar r;
    Random::seed(r, 131);

    Node node;
    u32 num_partial_overlaps = 0, num_partial_containments = 0;

    const auto check = [&](const Rect& q) {
        const auto count = node.branch_count;
        const auto overlapping = Tree::OverlappingBranches(node, q);
        const auto containing = Tree::ContainingBranches(node, q);
        fm_assert_equal(overlapping_branches_ref(node, q), overlapping);
        fm_assert_equal(containing_branches_ref(node, q), containing);
        fm_assert(count == 32 || (overlapping | containing) >> count == 0);
        const u32 used = count == 32 ? ~0u : (1u << count) - 1;
        num_partial_overlaps += overlapping != 0 && overlapping != used;
        num_partial_containments += containing != 0 && containing != used;
    };

    for (u32 iter = 0; iter < 2000; iter++)
    {
        const bool is_wide = iter % 4 == 0;
        const i32 lo = is_wide ? -16384 : -29, hi = is_wide ? 16000 : 23, max_size = is_wide ? 384 : 13;
        node.branch_count = (u32)uniform(r, 0, 32);
        node.level = 0;
        for (u32 i = 0; i < 32; i++)
        {
            set_branch_rect(node, i, i < node.branch_count ? random_rect(r, lo, hi, max_size) : Tree::empty_rect);
            node.id[i] = i;
        }

        for (u32 k = 0; k < 40; k++)
            check(random_rect(r, lo, hi, max_size * 2));
        for (u32 i = 0; i < node.branch_count; i++)
        {
            const auto b = branch_rect(node, i);
            check(b);
            check({b.max(), b.max() + Vec{5}});
            check({b.min() - Vec{7}, b.min()});
            check({b.min() + Vec{1}, b.max()});
        }
        check({Vec{(T)-search_max}, Vec{(T)search_max}});
        check({Vec{(T)search_max}, Vec{(T)search_max}});
        check({Vec{(T)-search_max}, Vec{(T)-search_max}});
    }

    fm_assert(num_partial_overlaps > 1000);
    fm_assert(num_partial_containments > 100);
}

template<typename T>
template<typename Compare>
bool rtree_tests<T>::overlaps_float(const Rect& e, const float min[2], const float max[2])
{
    for (u32 axis = 0; axis < 2; axis++)
        if (!Compare{}((double)e.min()[axis], (double)max[axis]) ||
            !Compare{}((double)min[axis], (double)e.max()[axis]))
            return false;
    return true;
}

template<typename T>
template<typename Compare>
bool rtree_tests<T>::overlaps_int(const Rect& e, Vector2i min, Vector2i max)
{
    for (u32 axis = 0; axis < 2; axis++)
        if (!Compare{}((i64)e.min()[axis], (i64)max[axis]) || !Compare{}((i64)min[axis], (i64)e.max()[axis]))
            return false;
    return true;
}

template<typename T>
template<typename Compare>
void rtree_tests<T>::check_search(const Tree& t, const Array<entry>& es, const float min[2], const float max[2],
                                  Array<u64>& found, Array<u64>& expected)
{
    arrayClear(found);
    arrayClear(expected);
    const auto num_found = t.template Search<Compare>(min, max, [&](u64 id, const Rect& r) {
        fm_assert(es[id].rect == r);
        arrayAppend(found, id);
        return true;
    });
    for (u32 i = 0; i < es.size(); i++)
        if (overlaps_float<Compare>(es[i].rect, min, max))
            arrayAppend(expected, (u64)i);
    check_same_ids(found, expected, num_found);
}

template<typename T>
template<typename Compare>
void rtree_tests<T>::check_search(const Tree& t, const Array<entry>& es, Vector2i min, Vector2i max,
                                  Array<u64>& found, Array<u64>& expected)
{
    arrayClear(found);
    arrayClear(expected);
    const auto num_found = t.template Search<Compare>(min, max, [&](u64 id, const Rect& r) {
        fm_assert(es[id].rect == r);
        arrayAppend(found, id);
        return true;
    });
    for (u32 i = 0; i < es.size(); i++)
        if (overlaps_int<Compare>(es[i].rect, min, max))
            arrayAppend(expected, (u64)i);
    check_same_ids(found, expected, num_found);
}

template<typename T>
void rtree_tests<T>::test_search()
{
    Random::xoshiro256starstar r;
    Random::seed(r, 137);
    Array<u64> found, expected;
    arrayReserve(found, 1100);
    arrayReserve(expected, 1100);
    u32 num_hits = 0;

    for (u32 n : { 0u, 1u, 32u, 33u, 1100u })
    {
        Tree t;
        Array<entry> es{ValueInit, n};
        for (u32 i = 0; i < n; i++)
        {
            es[i] = { random_rect(r, -600, 600, 64), i };
            t.Insert(es[i].rect, i);
        }
        fm_assert_equal(n, t.Count());

        const auto check_both = [&](const float min[2], const float max[2]) {
            check_search<cmp::less_equal<>>(t, es, min, max, found, expected);
            num_hits += (u32)found.size();
            check_search<cmp::less<>>(t, es, min, max, found, expected);
            num_hits += (u32)found.size();
        };
        const auto check_both_int = [&](Vector2i min, Vector2i max) {
            check_search<cmp::less_equal<>>(t, es, min, max, found, expected);
            num_hits += (u32)found.size();
            check_search<cmp::less<>>(t, es, min, max, found, expected);
            num_hits += (u32)found.size();
        };

        for (u32 k = 0; k < 400; k++)
        {
            Vector2 min, max;
            for (u32 axis = 0; axis < 2; axis++)
            {
                min[axis] = (float)uniform(r, -2800, 2800) / 4;
                max[axis] = k % 8 == 0 ? min[axis] : min[axis] + (float)uniform(r, 0, 800) / 4;
            }
            check_both(map_query_point(min).data(), map_query_point(max).data());

            const Vector2i imin{uniform(r, -700, 700), uniform(r, -700, 700)};
            const Vector2i isize = k % 8 == 0 ? Vector2i{} : Vector2i{uniform(r, 0, 200), uniform(r, 0, 200)};
            check_both_int(map_query_point(imin), map_query_point(imin + isize));
        }

        for (const auto& e : es)
        {
            const Vector2 lo{e.rect.min()}, hi{e.rect.max()};
            const Vector2 a = hi + Vector2{3}, b = lo - Vector2{3}, c = lo + Vector2{0.5f};
            check_both(hi.data(), a.data());
            check_both(b.data(), lo.data());
            check_both(lo.data(), hi.data());
            check_both(c.data(), c.data());
            check_both_int(Vector2i{e.rect.max()}, Vector2i{e.rect.max()} + Vector2i{3});
            check_both_int(Vector2i{e.rect.min()} - Vector2i{3}, Vector2i{e.rect.min()});
        }

        constexpr float d = sizeof(T) == 4 ? 1 << 29 : 8000;
        const float far_min[][2] = {
            {-search_max - d, -search_max - d}, {-search_max - d, map_float_coord(-5)},
            {search_max + .5f, map_float_coord(0)}, {-1e10f, -1e10f}, {-search_max - 1.5f, map_float_coord(-600)},
        };
        const float far_max[][2] = {
            {search_max + d, search_max + d}, {-search_max - 1.5f, map_float_coord(5)},
            {search_max + d, map_float_coord(0)}, {1e10f, 1e10f}, {-search_max - .5f, map_float_coord(600)},
        };
        for (u32 k = 0; k < 5; k++)
            check_both(far_min[k], far_max[k]);
        constexpr i32 lowest = limits<i32>::min, highest = limits<i32>::max;
        check_both_int(Vector2i{lowest}, Vector2i{highest});
        check_both_int({lowest, (i32)map_coord(-3)}, {(i32)(-search_max - 1), (i32)map_coord(3)});
        check_both_int({(i32)(search_max + 1), (i32)map_coord(-3)}, {highest, (i32)map_coord(3)});
    }

    fm_assert(num_hits > 10000);
}

template<typename T>
void rtree_tests<T>::test_search_bounds()
{
    constexpr i64 hi = Tree::max_coordinate, lo = -hi;
    constexpr i64 entry_coords[] = { lo, lo + 1, -1, 0, 1, hi - 1, hi };
    constexpr i64 query_coords[] = {
        limits<i32>::min, -search_max - 1, -search_max, -search_max + 1, lo - 1, lo, lo + 1, -1, 0, 1,
        hi - 1, hi, hi + 1, search_max - 1, search_max, search_max + 1, limits<i32>::max,
    };
    constexpr float fractions[] = { -.5, 0, .5 };

    Tree t;
    Array<entry> es;
    arrayReserve(es, 28);
    for (u32 i = 0; i < 7; i++)
        for (u32 j = i; j < 7; j++)
        {
            const entry e{{Vec{(T)entry_coords[i]}, Vec{(T)entry_coords[j]}}, (u64)es.size()};
            t.Insert(e.rect, e.id);
            arrayAppend(es, e);
        }

    Array<u64> found, expected;
    arrayReserve(found, 28);
    arrayReserve(expected, 28);
    u32 num_hits = 0, num_empty_searches = 0;
    const auto count = [&] {
        num_hits += (u32)found.size();
        num_empty_searches += found.isEmpty();
    };
    const auto check_both = [&](const float min[2], const float max[2]) {
        check_search<cmp::less_equal<>>(t, es, min, max, found, expected);
        count();
        check_search<cmp::less<>>(t, es, min, max, found, expected);
        count();
    };
    const auto check_both_int = [&](Vector2i min, Vector2i max) {
        check_search<cmp::less_equal<>>(t, es, min, max, found, expected);
        count();
        check_search<cmp::less<>>(t, es, min, max, found, expected);
        count();
    };

    for (i64 a : query_coords)
        for (i64 b : query_coords)
        {
            if (a > b)
                continue;
            check_both_int(Vector2i{(i32)a}, Vector2i{(i32)b});
            check_both_int({(i32)a, limits<i32>::min}, {(i32)b, limits<i32>::max});
            for (float fa : fractions)
                for (float fb : fractions)
                {
                    // Summed in float, because under fast math clang may replace (double)(float)x with x.
                    const float fmin = (float)a + fa, fmax = (float)b + fb;
                    if (fmin > fmax)
                        continue;
                    const float min[2] = { fmin, fmin }, max[2] = { fmax, fmax };
                    const float min_x[2] = { fmin, -1e10f }, max_x[2] = { fmax, 1e10f };
                    check_both(min, max);
                    check_both(min_x, max_x);
                }
        }

    fm_assert(num_hits > 100);
    fm_assert(num_empty_searches > 100);
}

template<typename T>
template<u32 N>
auto rtree_tests<T>::make_leaf(const Range2Di (&rects)[N]) -> Node
{
    static_assert(N <= 32);
    Node n;
    n.level = 0;
    n.branch_count = N;
    for (u32 i = 0; i < 32; i++)
    {
        set_branch_rect(n, i, i < N ? map_rect(rects[i]) : Tree::empty_rect);
        n.id[i] = i;
    }
    return n;
}

template<typename T>
void rtree_tests<T>::test_pick_branch()
{
    const auto rect = map_rect({{0, 0}, {10, 10}});

    fm_assert_equal(1u, Tree::PickBranch(rect, make_leaf({
        {{100, 100}, {110, 110}}, {{12, 0}, {20, 10}}, {{-50, -50}, {-40, -40}},
    })));
    fm_assert_equal(1u, Tree::PickBranch(rect, make_leaf({
        {{11, 0}, {14, 10}}, {{-1000, -1000}, {1000, 1000}}, {{100, 100}, {110, 110}},
    })));
    fm_assert_equal(3u, Tree::PickBranch(rect, make_leaf({
        {{-100, -100}, {100, 100}}, {{-20, -20}, {30, 30}}, {{50, 50}, {60, 60}}, {{-5, -5}, {15, 15}},
    })));
    fm_assert_equal(1u, Tree::PickBranch(rect, make_leaf({
        {{-100, -100}, {100, 100}}, {{-5, -5}, {15, 15}}, {{50, 50}, {60, 60}}, {{-6, -4}, {14, 16}},
        {{-20, -20}, {30, 30}},
    })));

    Range2Di rects[32];
    for (u32 i = 0; i < 32; i++)
        rects[i] = {{(i32)(100 + 20 * i), 100}, {(i32)(110 + 20 * i), 110}};
    rects[5] = {{-4, -6}, {16, 14}};
    rects[29] = {{-3, -2}, {17, 18}};
    fm_assert_equal(5u, Tree::PickBranch(rect, make_leaf(rects)));
    rects[31] = {{-2, -2}, {12, 12}};
    fm_assert_equal(31u, Tree::PickBranch(rect, make_leaf(rects)));
}

template<typename T>
void rtree_tests<T>::test_split_of_two_clusters()
{
    Random::xoshiro256starstar r;
    Random::seed(r, 157);

    for (u32 run = 0; run < 20; run++)
    {
        const u64 cluster_a_size = run % 2 == 0 ? 16 : 17;
        u64 order[33];
        for (u32 i = 0; i < 33; i++)
            order[i] = i;
        for (u32 i = 32; i > 0; i--)
            std::swap(order[i], order[(u32)uniform(r, 0, (i32)i)]);

        Tree t;
        for (u32 i = 0; i < 33; i++)
        {
            const auto id = order[i];
            const auto center = id < cluster_a_size ? Vector2i{-1000} : Vector2i{1000, 900};
            const auto min = center + Vector2i{uniform(r, -20, 20), uniform(r, -20, 20)};
            const Vector2i size{uniform(r, 0, 12), uniform(r, 0, 12)};
            t.Insert(map_rect({min, min + size}), id);
            fm_assert_equal(i < 32 ? 0u : 1u, t.raw_root()->level);
        }

        const auto& root = *t.raw_root();
        fm_assert_equal(2u, root.branch_count);
        for (u32 i = 0; i < 2; i++)
        {
            const auto& leaf = *root.child[i];
            const bool is_cluster_a = leaf.id[0] < cluster_a_size;
            fm_assert_equal(is_cluster_a ? (u32)cluster_a_size : 33 - (u32)cluster_a_size, leaf.branch_count);
            for (u32 j = 0; j < leaf.branch_count; j++)
                fm_assert((leaf.id[j] < cluster_a_size) == is_cluster_a);
        }
    }
}

template<typename T>
auto rtree_tests<T>::minimum_bounding_rect(const Node& n) -> Rect
{
    auto rect = Tree::empty_rect;
    for (u32 i = 0; i < n.branch_count; i++)
    {
        const auto b = branch_rect(n, i);
        rect = {Math::min(rect.min(), b.min()), Math::max(rect.max(), b.max())};
    }
    return rect;
}

template<typename T>
bool rtree_tests<T>::has_minimum_bounding_rects(const Node& n)
{
    if (n.level == 0)
        return true;
    else
    {
        for (u32 i = 0; i < n.branch_count; i++)
            if (branch_rect(n, i) != minimum_bounding_rect(*n.child[i]) || !has_minimum_bounding_rects(*n.child[i]))
                return false;
        return true;
    }
}

template<typename T>
void rtree_tests<T>::check_tree(const Node& n, bool is_root)
{
    fm_assert(n.branch_count <= 32);
    if (!is_root)
        fm_assert(n.branch_count >= Tree::MINNODES);
    else if (n.level > 0)
        fm_assert(n.branch_count >= 2);
    for (u32 i = 0; i < n.branch_count; i++)
        fm_assert((branch_rect(n, i).min() <= branch_rect(n, i).max()).all());
    for (u32 i = n.branch_count; i < 32; i++)
        fm_assert(branch_rect(n, i) == Tree::empty_rect);
    if (n.level > 0)
    {
        for (u32 i = 0; i < n.branch_count; i++)
        {
            const auto& child = *n.child[i];
            fm_assert(child.level == n.level - 1);
            for (u32 j = 0; j < child.branch_count; j++)
                fm_assert(contains(branch_rect(n, i), branch_rect(child, j)));
            check_tree(child, false);
        }
    }
}

template<typename T>
u32 rtree_tests<T>::check_leaf_entries(const Node& n, tree_state& s)
{
    u32 num_entries = 0;
    if (n.level > 0)
    {
        for (u32 i = 0; i < n.branch_count; i++)
            num_entries += check_leaf_entries(*n.child[i], s);
    }
    else
    {
        for (u32 i = 0; i < n.branch_count; i++)
        {
            const auto id = n.id[i];
            fm_assert(id < s.live_index_of_id.size());
            const auto index = s.live_index_of_id[id];
            fm_assert(index != no_index);
            fm_assert(!s.is_seen[id]);
            s.is_seen[id] = true;
            fm_assert(s.live[index].rect == branch_rect(n, i));
            num_entries++;
        }
    }
    return num_entries;
}

template<typename T>
void rtree_tests<T>::check_state(tree_state& s)
{
    const auto& root = *s.t.raw_root();
    check_tree(root);
    const bool is_tight = has_minimum_bounding_rects(root);
    fm_assert(is_tight || !s.needs_minimum_bounding_rects);
    s.needs_minimum_bounding_rects = is_tight;
    for (const auto& e : s.live)
        s.is_seen[e.id] = false;
    const auto num_entries = check_leaf_entries(*s.t.raw_root(), s);
    fm_assert_equal((u32)s.live.size(), num_entries);
    fm_assert_equal((u32)s.live.size(), s.t.Count());
}

template<typename T>
auto rtree_tests<T>::find_entry(const Node& n, u64 id) -> branch_pos
{
    if (n.level == 0)
    {
        for (u32 i = 0; i < n.branch_count; i++)
            if (n.id[i] == id)
                return { &n, i };
        return {};
    }
    else
    {
        for (u32 i = 0; i < n.branch_count; i++)
            if (auto x = find_entry(*n.child[i], id); x.node)
                return x;
        return {};
    }
}

template<typename T>
auto rtree_tests<T>::find_branch_to_leaf(const Node& n, u64 id) -> branch_pos
{
    fm_assert(n.level > 0);
    for (u32 i = 0; i < n.branch_count; i++)
    {
        const auto& child = *n.child[i];
        if (child.level > 0)
        {
            if (auto x = find_branch_to_leaf(child, id); x.node)
                return x;
        }
        else if (find_entry(child, id).node)
            return { &n, i };
    }
    return {};
}

template<typename T>
bool rtree_tests<T>::is_move_in_place(const Node& root, u64 id, const Rect& rect)
{
    if (root.level == 0)
        return true;
    else
    {
        const auto pos = find_branch_to_leaf(root, id);
        fm_assert(pos.node);
        return contains(branch_rect(*pos.node, pos.index), rect);
    }
}

template<typename T>
void rtree_tests<T>::insert_entry(tree_state& s, const entry& e)
{
    fm_assert(e.id < s.live_index_of_id.size());
    fm_assert(s.live_index_of_id[e.id] == no_index);
    s.t.Insert(e.rect, e.id);
    s.live_index_of_id[e.id] = (u32)s.live.size();
    arrayAppend(s.live, e);
}

template<typename T>
void rtree_tests<T>::remove_entry(tree_state& s, u32 index, const Rect& rect)
{
    const auto id = s.live[index].id;
    const auto num_nodes = Tree::node_pool.alive_count();
    const auto level = s.t.raw_root()->level;
    s.t.Remove(rect, id);
    s.num_removes_freeing_nodes += Tree::node_pool.alive_count() < num_nodes;
    s.num_root_level_drops += s.t.raw_root()->level < level;
    const auto last = s.live.back();
    s.live[index] = last;
    s.live_index_of_id[last.id] = index;
    s.live_index_of_id[id] = no_index;
    arrayRemoveSuffix(s.live, 1);
}

template<typename T>
void rtree_tests<T>::remove_entry(tree_state& s, u32 index)
{
    remove_entry(s, index, s.live[index].rect);
}

template<typename T>
void rtree_tests<T>::move_entry(tree_state& s, u32 index, const Rect& rect, u64 id)
{
    const auto e = s.live[index];
    fm_assert(id < s.live_index_of_id.size());
    fm_assert(id == e.id || s.live_index_of_id[id] == no_index);
    const auto num_nodes = Tree::node_pool.alive_count();
    const auto level = s.t.raw_root()->level;
    const auto pos0 = find_entry(*s.t.raw_root(), e.id);
    fm_assert(pos0.node);
    const bool is_in_place = is_move_in_place(*s.t.raw_root(), e.id, rect);
    const bool is_moved = s.t.Move(e.rect, e.id, rect, id);
    fm_assert(is_moved);
    const auto pos1 = find_entry(*s.t.raw_root(), id);
    fm_assert(pos1.node);
    if (is_in_place)
    {
        fm_assert(pos1.node == pos0.node && pos1.index == pos0.index);
        fm_assert(Tree::node_pool.alive_count() == num_nodes);
        s.needs_minimum_bounding_rects = false;
    }
    s.num_moves_in_place += is_in_place;
    s.num_moves_out_of_place += !is_in_place;
    s.num_moves_freeing_nodes += Tree::node_pool.alive_count() < num_nodes;
    s.num_root_level_drops += s.t.raw_root()->level < level;
    s.live_index_of_id[e.id] = no_index;
    s.live_index_of_id[id] = index;
    s.live[index] = { rect, id };
}

template<typename T>
void rtree_tests<T>::remove_all_entries(tree_state& s)
{
    s.t.RemoveAll();
    for (const auto& e : s.live)
        s.live_index_of_id[e.id] = no_index;
    arrayClear(s.live);
    s.needs_minimum_bounding_rects = true;
}

template<typename T>
bool rtree_tests<T>::is_id_less(const entry& a, const entry& b)
{
    return a.id < b.id;
}

template<typename T>
template<typename Compare>
void rtree_tests<T>::check_hits_with(tree_state& s, const float min[2], const float max[2])
{
    arrayClear(s.hits);
    const auto num_found = s.t.template Search<Compare>(min, max, [&](u64 id, const Rect& r) {
        arrayAppend(s.hits, entry{r, id});
        return true;
    });
    fm_assert_equal((usize)num_found, s.hits.size());

    for (u32 k : { 1u, 2u, 7u, (u32)s.hits.size() })
    {
        if (k == 0 || k > s.hits.size())
            continue;
        u32 num_calls = 0;
        u64 last_id = (u64)-1;
        const auto num_found_until_stop = s.t.template Search<Compare>(min, max, [&](u64 id, const Rect&) {
            last_id = id;
            return ++num_calls < k;
        });
        fm_assert_equal(k, num_found_until_stop);
        fm_assert_equal(k, num_calls);
        fm_assert_equal(s.hits[k - 1].id, last_id);
        s.num_early_stops++;
    }

    arrayClear(s.expected_hits);
    for (const auto& e : s.live)
        if (overlaps_float<Compare>(e.rect, min, max))
            arrayAppend(s.expected_hits, e);
    std::sort(s.hits.begin(), s.hits.end(), is_id_less);
    std::sort(s.expected_hits.begin(), s.expected_hits.end(), is_id_less);
    fm_assert_equal(s.expected_hits.size(), s.hits.size());
    for (u32 i = 0; i < s.hits.size(); i++)
    {
        fm_assert_equal(s.expected_hits[i].id, s.hits[i].id);
        fm_assert(s.expected_hits[i].rect == s.hits[i].rect);
    }
}

template<typename T>
void rtree_tests<T>::check_hits(tree_state& s, const float min[2], const float max[2])
{
    check_hits_with<cmp::less_equal<>>(s, min, max);
    check_hits_with<cmp::less<>>(s, min, max);
}

template<typename T>
void rtree_tests<T>::check_same(const Node& a, const Node& b)
{
    fm_assert(&a != &b);
    fm_assert(a.level == b.level && a.branch_count == b.branch_count);
    for (u32 i = 0; i < 32; i++)
        fm_assert(branch_rect(a, i) == branch_rect(b, i));
    for (u32 i = 0; i < a.branch_count; i++)
    {
        if (a.level == 0)
            fm_assert(a.id[i] == b.id[i]);
        else
            check_same(*a.child[i], *b.child[i]);
    }
}

template<typename T>
void rtree_tests<T>::check_copies(tree_state& s)
{
    Tree copy{s.t};
    check_same(*copy.raw_root(), *s.t.raw_root());
    Tree other;
    other.Insert(map_rect({Vector2i{3}, Vector2i{5}}), 11);
    other = s.t;
    check_same(*other.raw_root(), *s.t.raw_root());
    other.RemoveAll();
    fm_assert_equal(0u, other.Count());
    check_tree(*other.raw_root());
    fm_assert(has_minimum_bounding_rects(*other.raw_root()));
}

template<typename T>
void rtree_tests<T>::test_random_sequence(u64 seed, tree_moves moves)
{
    Random::xoshiro256starstar r;
    Random::seed(r, seed);
    tree_state s{1600};
    u64 next_id = 1;
    const Rect full_range_rect{Vec{limits<T>::min}, Vec{limits<T>::max}};

    const auto random_query = [&] {
        Vector2 min, max;
        for (u32 axis = 0; axis < 2; axis++)
        {
            min[axis] = (float)uniform(r, -2800, 2800) / 4;
            max[axis] = min[axis] + (float)uniform(r, 0, 600) / 4;
        }
        check_hits(s, map_query_point(min).data(), map_query_point(max).data());
    };

    for (u32 phase = 0; phase < 3; phase++)
    {
        const u32 insert_percent = phase == 1 ? 10 : 60, remove_percent = phase == 1 ? 60 : 10;
        const usize target = phase == 0 ? 1500 : phase == 1 ? 0 : 600;
        for (u32 step = 0; phase == 1 ? s.live.size() > target : s.live.size() < target; step++)
        {
            const auto op = (u32)uniform(r, 0, 99);
            if (op < insert_percent || s.live.isEmpty())
                insert_entry(s, { random_rect(r, -600, 600, 64), next_id++ });
            else if (op < insert_percent + remove_percent)
            {
                const auto index = (u32)uniform(r, 0, (i32)s.live.size() - 1);
                remove_entry(s, index, op % 7 == 0 ? full_range_rect : s.live[index].rect);
            }
            else if (op < 90)
            {
                const auto index = (u32)uniform(r, 0, (i32)s.live.size() - 1);
                const auto e = s.live[index];
                Rect rect;
                if (op % 4 == 0)
                    rect = random_rect(r, -600, 600, 64);
                else
                {
                    const Vector2i shift{uniform(r, -4, 4), uniform(r, -4, 4)};
                    const auto min = unmap_point(e.rect.min()) + shift;
                    rect = map_rect({min, unmap_point(e.rect.max()) + shift});
                    if (step % 16 == 0)
                    {
                        const Vector2i size{uniform(r, 0, 64), uniform(r, 0, 64)};
                        rect = map_rect(Range2Di::fromSize(min, size));
                    }
                }
                if (moves == tree_moves::out_of_place_only && s.t.raw_root()->level > 0)
                {
                    for (u32 i = 0; is_move_in_place(*s.t.raw_root(), e.id, rect); i++)
                    {
                        fm_assert(i < 100);
                        rect = random_rect(r, -600, 600, 64);
                    }
                }
                const auto id = op % 5 == 0 ? next_id++ : e.id;
                if (moves == tree_moves::none)
                {
                    remove_entry(s, index);
                    insert_entry(s, { rect, id });
                }
                else
                    move_entry(s, index, rect, id);
            }
            else if (op < 95)
                random_query();
            else if (op < 98)
            {
                const auto e = s.live[(u32)uniform(r, 0, (i32)s.live.size() - 1)];
                s.t.Remove(e.rect, next_id + 1000);
                fm_assert(!s.t.Move(e.rect, next_id + 1000, e.rect, next_id + 1001));
            }
            else
                check_copies(s);
            check_state(s);
        }
        for (u32 k = 0; k < 200; k++)
            random_query();
    }

    fm_assert(s.num_removes_freeing_nodes > 0);
    fm_assert(s.num_root_level_drops > 0);
    fm_assert(s.num_early_stops > 100);
    if (moves != tree_moves::none)
    {
        fm_assert(s.num_moves_freeing_nodes > 0);
        fm_assert(s.num_moves_out_of_place > 0);
    }
    if (moves == tree_moves::any)
        fm_assert(s.num_moves_in_place > 0);
    else
        fm_assert(s.needs_minimum_bounding_rects);

    remove_all_entries(s);
    check_state(s);
}

template<typename T>
void rtree_tests<T>::test_critter_sequence()
{
    Random::xoshiro256starstar r;
    Random::seed(r, 149);
    tree_state s{600};
    u64 next_id = 1;

    for (i32 y = 0; y < 16; y++)
        for (i32 x = 0; x < 16; x++)
        {
            const auto nw = Vector2i{x, y} * 64 - Vector2i{32};
            const auto kind = uniform(r, 0, 9);
            if (kind < 3)
                insert_entry(s, { map_rect({nw, nw + Vector2i{64, 8}}), next_id++ });
            if (kind >= 2 && kind < 5)
                insert_entry(s, { map_rect({nw, nw + Vector2i{8, 64}}), next_id++ });
            if (kind == 9)
                insert_entry(s, { map_rect({nw, nw + Vector2i{64}}), next_id++ });
            if (kind == 5)
            {
                const auto center = nw + Vector2i{uniform(r, 8, 56), uniform(r, 8, 56)};
                insert_entry(s, { map_rect({center - Vector2i{11}, center + Vector2i{13}}), next_id++ });
            }
        }
    check_state(s);

    const u32 first_critter = (u32)s.live.size();
    for (u32 i = 0; i < 6; i++)
    {
        const auto center = Vector2i{uniform(r, 0, 960), uniform(r, 0, 960)};
        insert_entry(s, { map_rect({center - Vector2i{16}, center + Vector2i{16}}), next_id++ });
    }
    check_state(s);

    for (u32 step = 0; step < 6000; step++)
    {
        const auto index = first_critter + (u32)uniform(r, 0, 5);
        const auto e = s.live[index];
        Vector2i shift{uniform(r, -3, 3), uniform(r, -3, 3)};
        if (step % 50 == 0)
            shift = Vector2i{uniform(r, -400, 400), uniform(r, -400, 400)};
        const auto min = Math::clamp(unmap_point(e.rect.min()) + shift, Vector2i{-64}, Vector2i{1024});
        move_entry(s, index, map_rect({min, min + Vector2i{32}}), e.id);
        check_state(s);
        if (step % 4 == 0)
        {
            const auto lo = map_query_point(Vector2{min} - Vector2{16.25f});
            const auto hi = map_query_point(Vector2{min + Vector2i{32}} + Vector2{15.75f});
            check_hits(s, lo.data(), hi.data());
        }
    }

    fm_assert(s.num_moves_in_place > 0);
    fm_assert(s.num_moves_out_of_place > 0);
    check_copies(s);
}

template<typename T>
void rtree_tests<T>::test_lattice_sequence()
{
    Random::xoshiro256starstar r;
    Random::seed(r, 151);
    tree_state s{600};
    u64 next_id = 1;

    const auto lattice_min = [](Vector2i cell) {
        return cell * 40 - Vector2i{480};
    };
    const auto lattice_rect = [&](Vector2i cell) {
        const auto min = lattice_min(cell);
        return map_rect({min, min + Vector2i{32}});
    };

    for (u32 order = 0; order < 2; order++)
    {
        for (i32 j = 0; j < 24; j++)
            for (i32 i = 0; i < 24; i++)
                insert_entry(s, { lattice_rect(order == 0 ? Vector2i{i, j} : Vector2i{j, i}), next_id++ });
        check_state(s);

        for (u32 step = 0; step < 400; step++)
        {
            const auto index = (u32)uniform(r, 0, (i32)s.live.size() - 1);
            const auto e = s.live[index];
            if (step % 3 == 0)
            {
                remove_entry(s, index);
                insert_entry(s, { lattice_rect(Vector2i{uniform(r, 0, 23), uniform(r, 0, 23)}), next_id++ });
            }
            else
            {
                const auto cell = (unmap_point(e.rect.min()) + Vector2i{480}) / 40;
                const Vector2i shift{uniform(r, -1, 1), uniform(r, -1, 1)};
                move_entry(s, index, lattice_rect(Math::clamp(cell + shift, Vector2i{0}, Vector2i{23})), e.id);
            }
            check_state(s);
            if (step % 8 == 0)
            {
                const auto min = lattice_min(Vector2i{uniform(r, 0, 23), uniform(r, 0, 23)});
                const auto lo = map_query_point(Vector2{min} - Vector2{20});
                const auto hi = map_query_point(Vector2{min + Vector2i{32}} + Vector2{20});
                check_hits(s, lo.data(), hi.data());
            }
        }

        remove_all_entries(s);
        check_state(s);
    }
}

template<typename T>
void rtree_tests<T>::run()
{
    test_kernels();
    test_search();
    test_search_bounds();
    test_pick_branch();
    test_split_of_two_clusters();
    test_random_sequence(139, tree_moves::any);
    test_random_sequence(163, tree_moves::none);
    test_random_sequence(173, tree_moves::out_of_place_only);
    test_critter_sequence();
    test_lattice_sequence();
}

} // namespace

void Test::test_simd_rtree()
{
    rtree_tests<i16>::run();
    rtree_tests<i32>::run();
}

} // namespace floormat
