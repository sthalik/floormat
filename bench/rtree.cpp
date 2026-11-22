#include "cover-world.hpp"
#include "raycast-dense-world.hpp"
#include "editor/mazes.hpp"
#include "random/random.hpp"
#include "src/RTree.hpp"
#include "src/RTree-search.hpp"
#include "src/rtree-pool.inl"
#include "src/chunk.hpp"
#include "src/grid-pass.hpp"
#include "src/tile-defs.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "loader/policy.hpp"
#include "compat/assert.hpp"
#include "compat/debug.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numbers>
#include <type_traits>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <cr/Path.h>
#include <cr/Pointer.h>
#include <cr/String.h>
#include <mg/Functions.h>
#include <mg/Range.h>
#include <benchmark/benchmark.h>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#endif
#include <hwy/highway.h>

namespace floormat {

namespace {

constexpr u32 max_lanes = 8;
constexpr i16 empty_rect_min = 32767, empty_rect_max = -32768;
constexpr float max_abs_coord = 16384;
constexpr i32 box_size = 32;
constexpr i32 whole_chunk_side = chunk_size<i32> + box_size;
constexpr i32 box_sides[] = { 0, box_size, 128, 512, whole_chunk_side };
constexpr u32 query_kind_count = (u32)std::size(box_sides);
constexpr u32 query_count = 4096;
constexpr u32 random_rect_entry_counts[] = { 8, 16, 32, 64, 96, 128, 160, 256, 512 };
constexpr u32 critter_count = 8;
constexpr u64 first_critter_id = ~u64{} - critter_count;

template<u32 M> using Chunk_RTree_Fanout = ::RTree<object_id, float, 2, float, (int)M, (int)M/2>;
static_assert(std::is_same_v<Chunk_RTree_Fanout<8>, Chunk_RTree>);

template<u32 M>
struct alignas(64) Node
{
    i16 m_min[2][M];
    i16 m_max[2][M];
    union { Node* m_child[M]; u64 m_data[M]; };
    int m_count, m_level;
};
static_assert(sizeof(Node<8>) == 192);

struct alignas(64) broadcast_rect
{
    i16 m_min[2][max_lanes];
    i16 m_max[2][max_lanes];
};

struct entry
{
    Math::Range2D<i16> rect;
    u64 data;

    bool operator==(const entry&) const = default;
};

// A search reads this cache line before the entries. Empty rects pad each array to a multiple of max_lanes.
struct alignas(64) entry_arrays
{
    i16 *min_x = nullptr, *min_y = nullptr, *max_x = nullptr, *max_y = nullptr;
    u64* data = nullptr;
    u32 count = 0;
    Array<i16> coord_storage[4];
    Array<u64> data_storage;
};

struct alignas(64) entry_block
{
    i16 min_x[max_lanes], min_y[max_lanes], max_x[max_lanes], max_y[max_lanes];
    u64 data[max_lanes];
};
static_assert(sizeof(entry_block) == 128);

struct chunk_structures
{
    Chunk_RTree_Fanout<8> old_tree_8;
    Chunk_RTree_Fanout<16> old_tree_16;
    Chunk_RTree_Fanout<32> old_tree_32;
    Array<Node<8>> nodes_8;
    Array<Node<16>> nodes_16;
    Array<Node<32>> nodes_32;
    const Node<8>* shuffled_root = nullptr;
    // The leaves of old_tree_8 in the order its search reports them.
    Array<entry> entries;
    Array<entry_block> blocks;
    entry_arrays arrays;
    chunk_coords_ coord;
};

struct query
{
    u32 chunk_index;
    float min[2], max[2];
    Range2Di rect;
};

struct chunks_and_queries
{
    Array<chunk_structures> chunks;
    u32 largest_chunk_index;
    // Indexed by whether every query goes to the largest chunk, then by the index of its side in box_sides.
    Array<query> queries[2][query_kind_count];
    u64 num_hits[2][query_kind_count];
};

struct benchmark_scene : chunks_and_queries
{
    Array<Node<8>> shuffled_nodes;
    Array<entry> largest_chunk_shuffled_entries;
};

enum class scene : u8 { dense, cover, maze, save, COUNT };
constexpr u32 scene_count = (u32)scene::COUNT;
constexpr const char* scene_names[scene_count] = { "dense", "cover", "maze", "save" };

template<u32 M, typename C>
auto& old_tree(C& c)
{
    if constexpr (M == 8)
        return c.old_tree_8;
    else if constexpr (M == 16)
        return c.old_tree_16;
    else
    {
        static_assert(M == 32);
        return c.old_tree_32;
    }
}

template<u32 M, typename C>
auto& nodes(C& c)
{
    if constexpr (M == 8)
        return c.nodes_8;
    else if constexpr (M == 16)
        return c.nodes_16;
    else
    {
        static_assert(M == 32);
        return c.nodes_32;
    }
}

u32 below(Random::xoshiro256starstar& rng, u32 n)
{
    return u32((Random::next(rng) >> 32) * n >> 32);
}

template<typename T>
void shuffle(ArrayView<T> xs, Random::xoshiro256starstar& rng)
{
    for (auto i = (u32)xs.size(); i > 1; i--)
        std::swap(xs[i - 1], xs[below(rng, i)]);
}

template<typename Rect>
Math::Range2D<i16> to_int16_rect(const Rect& r)
{
    for (const float x : { r.m_min[0], r.m_min[1], r.m_max[0], r.m_max[1] })
        fm_assert(Math::abs(x) <= max_abs_coord && x == (float)(i16)x);
    return {{(i16)r.m_min[0], (i16)r.m_min[1]}, {(i16)r.m_max[0], (i16)r.m_max[1]}};
}

u64 entry_hash(const entry& e)
{
    const auto& r = e.rect;
    const auto coords = (u64)(u16)r.min().x() | (u64)(u16)r.min().y() << 16 |
                        (u64)(u16)r.max().x() << 32 | (u64)(u16)r.max().y() << 48;
    return (e.data ^ coords*0x9e3779b97f4a7c15u) * 0xbf58476d1ce4e5b9u;
}

u64 hash_sum(ArrayView<const entry> entries)
{
    u64 sum = 0;
    for (const auto& e : entries)
        sum += entry_hash(e);
    return sum;
}

u64 sum_of_coord_bits(const Math::Range2D<i16>& r)
{
    return (u64)(u16)r.min().x() + (u16)r.min().y() + (u16)r.max().x() + (u16)r.max().y();
}

template<typename Rect>
u64 sum_of_coord_bits(const Rect& r)
{
    return (u64)std::bit_cast<u32>(r.m_min[0]) + std::bit_cast<u32>(r.m_min[1]) +
           std::bit_cast<u32>(r.m_max[0]) + std::bit_cast<u32>(r.m_max[1]);
}

template<u32 M>
Math::Range2D<i16> branch_rect(const Node<M>& node, u32 i)
{
    return {{node.m_min[0][i], node.m_min[1][i]}, {node.m_max[0][i], node.m_max[1][i]}};
}

template<typename OldNode>
u32 subtree_node_count(const OldNode& node)
{
    u32 n = 1;
    if (node.m_level > 0)
        for (int i = 0; i < node.m_count; i++)
            n += subtree_node_count(*node.m_branch[i].m_child);
    return n;
}

// Returns the index after the last node written.
template<u32 M, typename OldNode>
u32 copy_subtree(Node<M>* nodes, u32 index, const OldNode& from)
{
    auto& to = nodes[index++];
    to.m_count = from.m_count;
    to.m_level = from.m_level;
    for (u32 i = 0; i < M; i++)
    {
        to.m_min[0][i] = to.m_min[1][i] = empty_rect_min;
        to.m_max[0][i] = to.m_max[1][i] = empty_rect_max;
        to.m_data[i] = 0;
    }
    for (u32 i = 0; i < (u32)from.m_count; i++)
    {
        const auto& b = from.m_branch[i];
        const auto r = to_int16_rect(b.m_rect);
        to.m_min[0][i] = r.min().x();
        to.m_min[1][i] = r.min().y();
        to.m_max[0][i] = r.max().x();
        to.m_max[1][i] = r.max().y();
        if (from.m_level > 0)
        {
            to.m_child[i] = &nodes[index];
            index = copy_subtree(nodes, index, *b.m_child);
        }
        else
            to.m_data[i] = b.m_data;
    }
    return index;
}

template<u32 M>
Array<Node<M>> copy_tree(const Chunk_RTree_Fanout<M>& tree)
{
    const auto& root = *tree.raw_root();
    Array<Node<M>> ret{ValueInit, subtree_node_count(root)};
    const auto num_copied = copy_subtree(ret.data(), 0, root);
    fm_assert(num_copied == ret.size());
    return ret;
}

template<typename OldNode>
void append_subtree_entries(Array<entry>& entries, const OldNode& node)
{
    for (int i = 0; i < node.m_count; i++)
    {
        const auto& b = node.m_branch[i];
        if (node.m_level > 0)
            append_subtree_entries(entries, *b.m_child);
        else
            arrayAppend(entries, entry{to_int16_rect(b.m_rect), b.m_data});
    }
}

template<typename Tree>
Array<entry> leaf_entries(const Tree& tree)
{
    const auto count = (u32)tree.Count();
    Array<entry> ret;
    arrayReserve(ret, Math::max(count, 16u));
    append_subtree_entries(ret, *tree.raw_root());
    fm_assert(ret.size() == count);
    return ret;
}

template<typename Tree>
void insert_entries(Tree& tree, ArrayView<const entry> entries)
{
    for (const auto& e : entries)
    {
        const float min[2] = {(float)e.rect.min().x(), (float)e.rect.min().y()};
        const float max[2] = {(float)e.rect.max().x(), (float)e.rect.max().y()};
        tree.Insert(min, max, e.data);
    }
}

void update_pointers(entry_arrays& a)
{
    a.min_x = a.coord_storage[0].data();
    a.min_y = a.coord_storage[1].data();
    a.max_x = a.coord_storage[2].data();
    a.max_y = a.coord_storage[3].data();
    a.data = a.data_storage.data();
}

void set_empty_slot(entry_arrays& a, u32 i)
{
    a.min_x[i] = a.min_y[i] = empty_rect_min;
    a.max_x[i] = a.max_y[i] = empty_rect_max;
    a.data[i] = 0;
}

void store_entry(entry_arrays& a, u32 i, const Math::Range2D<i16>& rect, u64 data)
{
    a.min_x[i] = rect.min().x();
    a.min_y[i] = rect.min().y();
    a.max_x[i] = rect.max().x();
    a.max_y[i] = rect.max().y();
    a.data[i] = data;
}

void fill_entry_arrays(entry_arrays& a, ArrayView<const entry> entries)
{
    const auto n = (u32)entries.size(), padded = (n + max_lanes - 1)/max_lanes*max_lanes;
    for (auto& s : a.coord_storage)
        arrayResize(s, NoInit, padded);
    arrayResize(a.data_storage, NoInit, padded);
    update_pointers(a);
    for (u32 i = 0; i < n; i++)
        store_entry(a, i, entries[i].rect, entries[i].data);
    for (u32 i = n; i < padded; i++)
        set_empty_slot(a, i);
    a.count = n;
}

void insert_entry(entry_arrays& a, const Math::Range2D<i16>& rect, u64 data)
{
    const auto n = a.count;
    if (n == a.data_storage.size())
    {
        for (auto& s : a.coord_storage)
            arrayResize(s, NoInit, n + max_lanes);
        arrayResize(a.data_storage, NoInit, n + max_lanes);
        update_pointers(a);
        for (u32 i = n; i < n + max_lanes; i++)
            set_empty_slot(a, i);
    }
    store_entry(a, n, rect, data);
    a.count = n + 1;
}

Array<entry_block> make_entry_blocks(ArrayView<const entry> entries)
{
    const auto n = (u32)entries.size();
    Array<entry_block> blocks{ValueInit, (n + max_lanes - 1)/max_lanes};
    for (u32 i = 0; i < blocks.size()*max_lanes; i++)
    {
        auto& b = blocks[i / max_lanes];
        const auto k = i % max_lanes;
        if (i < n)
        {
            const auto& r = entries[i].rect;
            b.min_x[k] = r.min().x();
            b.min_y[k] = r.min().y();
            b.max_x[k] = r.max().x();
            b.max_y[k] = r.max().y();
            b.data[k] = entries[i].data;
        }
        else
        {
            b.min_x[k] = b.min_y[k] = empty_rect_min;
            b.max_x[k] = b.max_y[k] = empty_rect_max;
            b.data[k] = 0;
        }
    }
    return blocks;
}

broadcast_rect make_broadcast_rect(const Math::Range2D<i16>& r)
{
    broadcast_rect ret;
    for (u32 i = 0; i < max_lanes; i++)
    {
        ret.m_min[0][i] = r.min().x();
        ret.m_min[1][i] = r.min().y();
        ret.m_max[0][i] = r.max().x();
        ret.m_max[1][i] = r.max().y();
    }
    return ret;
}

// Returns q clamped to ±32766. A query reaching the empty rect's values would match unused branches.
Math::Range2D<i16> SearchRect(const Range2Di& q)
{
    constexpr i32 bound = 32766;
    const auto clamp = [](i32 x) { return (i16)Math::clamp(x, -bound, bound); };
    return {{clamp(q.min().x()), clamp(q.min().y())}, {clamp(q.max().x()), clamp(q.max().y())}};
}

Math::Range2D<i16> SearchRect(const float a_min[2], const float a_max[2])
{
    constexpr float bound = 32766;
    const auto round_min = [](float x) { return (i16)Math::clamp(Math::ceil(x), -bound, bound); };
    const auto round_max = [](float x) { return (i16)Math::clamp(Math::floor(x), -bound, bound); };
    return {{round_min(a_min[0]), round_min(a_min[1])}, {round_max(a_max[0]), round_max(a_max[1])}};
}

} // namespace

} // namespace floormat

HWY_BEFORE_NAMESPACE();
namespace floormat::HWY_NAMESPACE {
namespace {

namespace hn = hwy::HWY_NAMESPACE;

template<u32 M>
u32 OverlappingBranches(const Node<M>& node, const broadcast_rect& rect)
{
#if HWY_MAX_BYTES <= 64
    constexpr hn::CappedTag<i16, max_lanes> d;
    constexpr auto N = (u32)hn::MaxLanes(d);
    static_assert(M % N == 0 && M <= 32);
    const auto q_min_x = hn::Load(d, rect.m_min[0]), q_min_y = hn::Load(d, rect.m_min[1]);
    const auto q_max_x = hn::Load(d, rect.m_max[0]), q_max_y = hn::Load(d, rect.m_max[1]);
    u32 disjoint_bits = 0;
    for (u32 i = 0; i < M; i += N)
    {
        const auto disjoint = hn::Or(hn::Or(hn::Gt(hn::Load(d, node.m_min[0] + i), q_max_x),
                                            hn::Gt(hn::Load(d, node.m_min[1] + i), q_max_y)),
                                     hn::Or(hn::Lt(hn::Load(d, node.m_max[0] + i), q_min_x),
                                            hn::Lt(hn::Load(d, node.m_max[1] + i), q_min_y)));
        disjoint_bits |= (u32)hn::BitsFromMask(d, disjoint) << i;
    }
    return ~disjoint_bits & (u32)((u64{1} << M) - 1);
#else
    u32 bits = 0;
    for (u32 i = 0; i < M; i++)
        bits |= (u32)(node.m_min[0][i] <= rect.m_max[0][0] && node.m_min[1][i] <= rect.m_max[1][0] &&
                      node.m_max[0][i] >= rect.m_min[0][0] && node.m_max[1][i] >= rect.m_min[1][0]) << i;
    return bits;
#endif
}

template<u32 M, typename F>
bool search_node(const Node<M>& node, int level, const broadcast_rect& rect, F& callback)
{
    auto bits = OverlappingBranches(node, rect);
    fm_debug3_assert(!((u64)bits >> node.m_count));
    if (level > 0)
    {
        for (; bits; bits &= bits - 1)
            if (!search_node(*node.m_child[std::countr_zero(bits)], level - 1, rect, callback))
                return false;
    }
    else
        for (; bits; bits &= bits - 1)
        {
            const auto i = (u32)std::countr_zero(bits);
            if (!callback(node.m_data[i], branch_rect(node, i)))
                return false;
        }
    return true;
}

template<u32 M, typename F>
int search_tree(const Node<M>& root, const Range2Di& query, F&& callback)
{
    const auto rect = make_broadcast_rect(SearchRect(query));
    int num_found = 0;
    const auto count_and_call = [&](u64 data, const Math::Range2D<i16>& r) HWY_ATTR {
        num_found++;
        return callback(data, r);
    };
    search_node(root, root.m_level, rect, count_and_call);
    return num_found;
}

template<u32 M, typename F>
int search_tree_with_float_query(const Node<M>& root, const float a_min[2], const float a_max[2], F&& callback)
{
    const auto rect = make_broadcast_rect(SearchRect(a_min, a_max));
    int num_found = 0;
    const auto count_and_call = [&](u64 data, const Math::Range2D<i16>& r) HWY_ATTR {
        num_found++;
        return callback(data, r);
    };
    search_node(root, root.m_level, rect, count_and_call);
    return num_found;
}

// Reports in the order search_tree() does.
template<u32 M, typename F>
int search_tree_with_stack(const Node<M>& root, const Range2Di& query, F&& callback)
{
    struct node_bits { const Node<M>* node; u32 bits; };
    constexpr u32 max_height = 16;
    const auto rect = make_broadcast_rect(SearchRect(query));
    const auto leaf_depth = (u32)root.m_level;
    fm_debug3_assert(leaf_depth < max_height);
    node_bits stack[max_height];
    stack[0] = {&root, OverlappingBranches(root, rect)};
    u32 depth = 0;
    int num_found = 0;
    for (;;)
    {
        auto& top = stack[depth];
        if (depth < leaf_depth && top.bits)
        {
            const auto i = (u32)std::countr_zero(top.bits);
            top.bits &= top.bits - 1;
            const auto& child = *top.node->m_child[i];
            stack[++depth] = {&child, OverlappingBranches(child, rect)};
        }
        else
        {
            if (depth == leaf_depth)
                for (auto bits = top.bits; bits; bits &= bits - 1)
                {
                    const auto i = (u32)std::countr_zero(bits);
                    num_found++;
                    if (!callback(top.node->m_data[i], branch_rect(*top.node, i)))
                        return num_found;
                }
            if (depth == 0)
                return num_found;
            else
                depth--;
        }
    }
}

template<typename F>
int search_arrays(const entry_arrays& a, const Range2Di& query, F&& callback)
{
    const auto rect = SearchRect(query);
    const auto n = a.count;
    const i16 *const min_x = a.min_x, *const min_y = a.min_y, *const max_x = a.max_x, *const max_y = a.max_y;
    const u64* const data = a.data;
    int num_found = 0;
#if HWY_MAX_BYTES <= 64
    constexpr hn::CappedTag<i16, max_lanes> d;
    constexpr auto N = (u32)hn::MaxLanes(d);
    const auto q_min_x = hn::Set(d, rect.min().x()), q_min_y = hn::Set(d, rect.min().y());
    const auto q_max_x = hn::Set(d, rect.max().x()), q_max_y = hn::Set(d, rect.max().y());
    for (u32 i = 0; i < n; i += N)
    {
        const auto disjoint = hn::Or(hn::Or(hn::Gt(hn::LoadU(d, min_x + i), q_max_x),
                                            hn::Gt(hn::LoadU(d, min_y + i), q_max_y)),
                                     hn::Or(hn::Lt(hn::LoadU(d, max_x + i), q_min_x),
                                            hn::Lt(hn::LoadU(d, max_y + i), q_min_y)));
        auto bits = ~(u32)hn::BitsFromMask(d, disjoint) & ((1u << N) - 1);
        for (; bits; bits &= bits - 1)
        {
            const auto k = i + (u32)std::countr_zero(bits);
            num_found++;
            if (!callback(data[k], Math::Range2D<i16>{{min_x[k], min_y[k]}, {max_x[k], max_y[k]}}))
                return num_found;
        }
    }
#else
    for (u32 k = 0; k < n; k++)
        if (min_x[k] <= rect.max().x() && min_y[k] <= rect.max().y() &&
            max_x[k] >= rect.min().x() && max_y[k] >= rect.min().y())
        {
            num_found++;
            if (!callback(data[k], Math::Range2D<i16>{{min_x[k], min_y[k]}, {max_x[k], max_y[k]}}))
                return num_found;
        }
#endif
    return num_found;
}

template<typename F>
int search_blocks(ArrayView<const entry_block> blocks, const Range2Di& query, F&& callback)
{
    const auto rect = SearchRect(query);
    int num_found = 0;
#if HWY_MAX_BYTES <= 64
    constexpr hn::CappedTag<i16, max_lanes> d;
    constexpr auto N = (u32)hn::MaxLanes(d);
    const auto q_min_x = hn::Set(d, rect.min().x()), q_min_y = hn::Set(d, rect.min().y());
    const auto q_max_x = hn::Set(d, rect.max().x()), q_max_y = hn::Set(d, rect.max().y());
    for (const auto& b : blocks)
        for (u32 i = 0; i < max_lanes; i += N)
        {
            const auto disjoint = hn::Or(hn::Or(hn::Gt(hn::Load(d, b.min_x + i), q_max_x),
                                                hn::Gt(hn::Load(d, b.min_y + i), q_max_y)),
                                         hn::Or(hn::Lt(hn::Load(d, b.max_x + i), q_min_x),
                                                hn::Lt(hn::Load(d, b.max_y + i), q_min_y)));
            auto bits = ~(u32)hn::BitsFromMask(d, disjoint) & ((1u << N) - 1);
            for (; bits; bits &= bits - 1)
            {
                const auto k = i + (u32)std::countr_zero(bits);
                num_found++;
                if (!callback(b.data[k], Math::Range2D<i16>{{b.min_x[k], b.min_y[k]}, {b.max_x[k], b.max_y[k]}}))
                    return num_found;
            }
        }
#else
    for (const auto& b : blocks)
        for (u32 k = 0; k < max_lanes; k++)
            if (b.min_x[k] <= rect.max().x() && b.min_y[k] <= rect.max().y() &&
                b.max_x[k] >= rect.min().x() && b.max_y[k] >= rect.min().y())
            {
                num_found++;
                if (!callback(b.data[k], Math::Range2D<i16>{{b.min_x[k], b.min_y[k]}, {b.max_x[k], b.max_y[k]}}))
                    return num_found;
            }
#endif
    return num_found;
}

// Returns the index of the entry with min corner `min` and data `data`, or a.count.
u32 find_entry(const entry_arrays& a, const Math::Vector2<i16>& min, u64 data)
{
    const auto n = a.count;
#if HWY_MAX_BYTES <= 64
    constexpr hn::CappedTag<i16, max_lanes> d;
    constexpr auto N = (u32)hn::MaxLanes(d);
    const auto min_x = hn::Set(d, min.x()), min_y = hn::Set(d, min.y());
    for (u32 i = 0; i < n; i += N)
    {
        const auto same_corner = hn::And(hn::Eq(hn::LoadU(d, a.min_x + i), min_x),
                                         hn::Eq(hn::LoadU(d, a.min_y + i), min_y));
        for (auto bits = (u32)hn::BitsFromMask(d, same_corner); bits; bits &= bits - 1)
        {
            const auto k = i + (u32)std::countr_zero(bits);
            if (k < n && a.data[k] == data)
                return k;
        }
    }
#else
    for (u32 k = 0; k < n; k++)
        if (a.min_x[k] == min.x() && a.min_y[k] == min.y() && a.data[k] == data)
            return k;
#endif
    return n;
}

bool move_entry(entry_arrays& a, const Math::Range2D<i16>& from, u64 data, const Math::Range2D<i16>& to)
{
    const auto k = find_entry(a, from.min(), data);
    if (k == a.count)
        return false;
    else
    {
        store_entry(a, k, to, data);
        return true;
    }
}

// Keeps the other entries in order.
bool remove_entry(entry_arrays& a, const Math::Range2D<i16>& rect, u64 data)
{
    const auto k = find_entry(a, rect.min(), data);
    if (k == a.count)
        return false;
    else
    {
        const auto n = --a.count;
        for (u32 i = k; i < n; i++)
        {
            a.min_x[i] = a.min_x[i + 1];
            a.min_y[i] = a.min_y[i + 1];
            a.max_x[i] = a.max_x[i + 1];
            a.max_y[i] = a.max_y[i + 1];
            a.data[i] = a.data[i + 1];
        }
        set_empty_slot(a, n);
        return true;
    }
}

} // namespace
} // namespace floormat::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

namespace floormat {

namespace {

using HWY_NAMESPACE::search_tree;
using HWY_NAMESPACE::search_tree_with_float_query;
using HWY_NAMESPACE::search_tree_with_stack;
using HWY_NAMESPACE::search_arrays;
using HWY_NAMESPACE::search_blocks;
using HWY_NAMESPACE::move_entry;
using HWY_NAMESPACE::remove_entry;

Optional<world> make_scene_world(scene s)
{
    switch (s)
    {
    case scene::dense:
        return make_dense_world();
    case scene::cover: {
        auto w = world();
        make_cover_world(w);
        return Optional<world>{move(w)};
    }
    case scene::maze: {
        auto w = world();
        {
            Pass::Pool pool{Pass::Params{16, mazes::bbox_size, pass_mode::pass}};
            mazes::build(w, mazes::kind::prim, chunk_coords_{0, 0, 0}, pool);
        }
        return Optional<world>{move(w)};
    }
    case scene::save: {
        const auto path = Path::join(loader.TEMP_PATH, "bench/save/proto-32-quicksave.dat"_s);
        if (!Path::exists(path))
            return NullOpt;
        else
            return world::deserialize(path, loader_policy::warn);
    }
    case scene::COUNT:
        break;
    }
    fm_assert(false);
}

// c.old_tree_8 must hold the chunk's entries. The trees of fan-out 16 and 32 get them in insertion_order.
void fill_structures(chunk_structures& c, ArrayView<const entry> insertion_order)
{
    c.entries = leaf_entries(c.old_tree_8);
    insert_entries(c.old_tree_16, insertion_order);
    insert_entries(c.old_tree_32, insertion_order);
    c.nodes_8 = copy_tree<8>(c.old_tree_8);
    c.nodes_16 = copy_tree<16>(c.old_tree_16);
    c.nodes_32 = copy_tree<32>(c.old_tree_32);
    c.blocks = make_entry_blocks(c.entries);
    fill_entry_arrays(c.arrays, c.entries);
}

// Returns every chunk's nodes_8 in one array in a random order, and points each chunk's shuffled_root into it.
Array<Node<8>> make_shuffled_nodes(ArrayView<chunk_structures> chunks, Random::xoshiro256starstar& rng)
{
    u32 node_count = 0;
    for (const auto& c : chunks)
        node_count += (u32)c.nodes_8.size();
    Array<u32> slots{NoInit, node_count};
    for (u32 i = 0; i < node_count; i++)
        slots[i] = i;
    shuffle(arrayView(slots), rng);
    Array<Node<8>> ret{ValueInit, node_count};
    u32 base = 0;
    for (auto& c : chunks)
    {
        const auto* const first = c.nodes_8.data();
        for (u32 j = 0; j < c.nodes_8.size(); j++)
        {
            const auto& from = c.nodes_8[j];
            auto& to = ret[slots[base + j]];
            to = from;
            if (from.m_level > 0)
                for (u32 i = 0; i < (u32)from.m_count; i++)
                    to.m_child[i] = &ret[slots[base + (u32)(from.m_child[i] - first)]];
        }
        c.shuffled_root = &ret[slots[base]];
        base += (u32)c.nodes_8.size();
    }
    return ret;
}

template<typename Tree>
Array<entry> old_tree_hits(const Tree& tree, const float min[2], const float max[2], usize max_count)
{
    Array<entry> hits;
    arrayReserve(hits, Math::max(max_count, usize{16}));
    tree.Search(min, max, [&](u64 data, const auto& r) {
        arrayAppend(hits, entry{to_int16_rect(r), data});
        return true;
    });
    return hits;
}

template<typename Search>
void check_hits_in_order(ArrayView<const entry> expected, const Search& search)
{
    u32 i = 0;
    const int count = search([&](u64 data, const Math::Range2D<i16>& r) {
        fm_assert(i < expected.size() && expected[i] == entry{r, data});
        i++;
        return true;
    });
    fm_assert(count == (int)expected.size() && i == expected.size());
}

// Returns the number of entries the old tree reports for q, whose float and integer bounds must be equal.
u32 check_query(const chunk_structures& c, const query& q)
{
    const auto n = c.entries.size();
    const auto hits_8 = old_tree_hits(c.old_tree_8, q.min, q.max, n);
    const auto hits_16 = old_tree_hits(c.old_tree_16, q.min, q.max, n);
    const auto hits_32 = old_tree_hits(c.old_tree_32, q.min, q.max, n);
    fm_assert(hits_16.size() == hits_8.size() && hash_sum(hits_16) == hash_sum(hits_8));
    fm_assert(hits_32.size() == hits_8.size() && hash_sum(hits_32) == hash_sum(hits_8));

    const auto& r = q.rect;
    const auto& root = c.nodes_8.front();
    check_hits_in_order(hits_8, [&](auto&& f) { return search_tree(root, r, f); });
    check_hits_in_order(hits_16, [&](auto&& f) { return search_tree(c.nodes_16.front(), r, f); });
    check_hits_in_order(hits_32, [&](auto&& f) { return search_tree(c.nodes_32.front(), r, f); });
    if (c.shuffled_root)
        check_hits_in_order(hits_8, [&](auto&& f) { return search_tree(*c.shuffled_root, r, f); });
    check_hits_in_order(hits_8, [&](auto&& f) { return search_tree_with_stack(root, r, f); });
    check_hits_in_order(hits_8, [&](auto&& f) { return search_arrays(c.arrays, r, f); });
    check_hits_in_order(hits_8, [&](auto&& f) { return search_blocks(c.blocks, r, f); });
    check_hits_in_order(hits_8, [&](auto&& f) { return search_tree_with_float_query(root, q.min, q.max, f); });
    return (u32)hits_8.size();
}

void check_float_query(const chunk_structures& c, const float min[2], const float max[2])
{
    const auto& root = c.nodes_8.front();
    const auto hits = old_tree_hits(c.old_tree_8, min, max, c.entries.size());
    check_hits_in_order(hits, [&](auto&& f) { return search_tree_with_float_query(root, min, max, f); });
}

query make_query(u32 chunk_index, Vector2i min, Vector2i max)
{
    return {
        .chunk_index = chunk_index,
        .min = {(float)min.x(), (float)min.y()},
        .max = {(float)max.x(), (float)max.y()},
        .rect = {min, max},
    };
}

// Moves each bound of q by up to 1 in steps of 1/8. On a point query the rounded bounds can then cross.
void check_fractional_query(const chunk_structures& c, const query& q, Random::xoshiro256starstar& rng)
{
    float min[2], max[2];
    for (u32 k = 0; k < 2; k++)
    {
        min[k] = q.min[k] + ((float)below(rng, 17) - 8)/8;
        max[k] = q.max[k] + ((float)below(rng, 17) - 8)/8;
        if (min[k] > max[k])
            std::swap(min[k], max[k]);
    }
    check_float_query(c, min, max);
}

// Checks queries that touch an entry's edges or reach past the int16 range.
void check_edge_queries(const chunk_structures& c, Random::xoshiro256starstar& rng)
{
    constexpr i32 far = 40000;
    const Vector2i far_bounds[][2] = {
        {Vector2i{-far}, Vector2i{far}},
        {Vector2i{-32768}, Vector2i{32767}},
        {Vector2i{33000}, Vector2i{34000}},
        {Vector2i{-34000}, Vector2i{-33000}},
        {{-far, 0}, {far, 0}},
        {{2000, 2000}, {3000, 3000}},
    };
    for (const auto& b : far_bounds)
        check_query(c, make_query(0, b[0], b[1]));
    const auto n = (u32)c.entries.size();
    for (u32 i = 0; i < Math::min(n, 16u); i++)
    {
        const auto& r = c.entries[below(rng, n)].rect;
        const auto min = Vector2i{r.min()}, max = Vector2i{r.max()};
        for (const auto& [a, b] : { std::pair{max, max + Vector2i{10}}, std::pair{min - Vector2i{10}, min},
                                    std::pair{min, max}, std::pair{min, min}, std::pair{max, max},
                                    std::pair{Vector2i{min.x(), max.y()}, Vector2i{min.x(), max.y()}} })
            check_query(c, make_query(0, a, b));
    }
}

Array<Chunk_RTree::Rect> box_rects_on_circle()
{
    constexpr u32 point_count = 1024;
    constexpr float radius = 300, half_box = box_size/2;
    constexpr auto center = chunk_size<float>/2 - half_tile<float>;
    Array<Chunk_RTree::Rect> rects{ValueInit, point_count};
    for (u32 i = 0; i < point_count; i++)
    {
        const auto angle = 2*std::numbers::pi_v<float>*(float)i/(float)point_count;
        const auto x = Math::round(center + radius*std::cos(angle)), y = Math::round(center + radius*std::sin(angle));
        rects[i] = {{x - half_box, y - half_box}, {x + half_box, y + half_box}};
    }
    return rects;
}

Array<Math::Range2D<i16>> int16_box_rects_on_circle()
{
    const auto float_rects = box_rects_on_circle();
    Array<Math::Range2D<i16>> rects{ValueInit, float_rects.size()};
    for (u32 i = 0; i < rects.size(); i++)
        rects[i] = to_int16_rect(float_rects[i]);
    return rects;
}

void check_arrays_equal(const entry_arrays& a, ArrayView<const entry> list)
{
    const auto padded = (u32)a.data_storage.size();
    fm_assert(a.count == list.size() && a.count <= padded && padded % max_lanes == 0);
    for (u32 i = 0; i < padded; i++)
    {
        const auto r = Math::Range2D<i16>{{a.min_x[i], a.min_y[i]}, {a.max_x[i], a.max_y[i]}};
        if (i < a.count)
            fm_assert(r == list[i].rect && a.data[i] == list[i].data);
        else
            fm_assert(r == Math::Range2D<i16>{Math::Vector2<i16>{empty_rect_min}, Math::Vector2<i16>{empty_rect_max}});
    }
}

// Moves and reinserts critters as the update benchmarks do, checking the arrays against a list after each.
void check_array_updates(const chunk_structures& c, Random::xoshiro256starstar& rng)
{
    const auto rects = int16_box_rects_on_circle();
    const auto point_count = (u32)rects.size();
    Array<entry> list;
    arrayReserve(list, Math::max(c.entries.size() + critter_count, usize{16}));
    arrayAppend(list, ArrayView<const entry>{c.entries});
    u32 positions[critter_count];
    for (u32 j = 0; j < critter_count; j++)
    {
        positions[j] = j * (point_count / critter_count);
        arrayAppend(list, entry{rects[positions[j]], first_critter_id + j});
    }
    entry_arrays a;
    fill_entry_arrays(a, list);
    check_arrays_equal(a, list);
    for (u32 iter = 0; iter < 2048; iter++)
    {
        const auto j = below(rng, critter_count);
        const auto id = first_critter_id + j;
        const auto& from = rects[positions[j]];
        positions[j] = (positions[j] + 1) % point_count;
        const auto& to = rects[positions[j]];
        u32 k = 0;
        while (list[k].data != id)
            k++;
        if (below(rng, 2))
        {
            fm_assert(move_entry(a, from, id, to));
            list[k].rect = to;
        }
        else
        {
            fm_assert(remove_entry(a, from, id));
            arrayRemove(list, k);
            insert_entry(a, to, id);
            arrayAppend(list, entry{to, id});
        }
        check_arrays_equal(a, list);
    }
}

Array<query> make_queries(u32 chunk_count, u32 largest_chunk_index, bool is_largest_only, i32 box_side,
                          Random::xoshiro256starstar& rng)
{
    Array<query> queries{ValueInit, query_count};
    for (auto& q : queries)
    {
        Vector2i min, max;
        if (box_side == whole_chunk_side)
        {
            min = Vector2i{-half_tile<i32> - box_size/2};
            max = min + Vector2i{whole_chunk_side};
        }
        else
        {
            const auto center = Vector2i{(i32)below(rng, chunk_size_xy), (i32)below(rng, chunk_size_xy)} -
                                half_tile<Vector2i>;
            min = center - Vector2i{box_side/2};
            max = min + Vector2i{box_side};
        }
        q = make_query(is_largest_only ? largest_chunk_index : below(rng, chunk_count), min, max);
    }
    return queries;
}

void make_checked_queries(chunks_and_queries& s, u32 kind, Random::xoshiro256starstar& rng)
{
    for (u32 is_largest_only = 0; is_largest_only < 2; is_largest_only++)
    {
        auto& queries = s.queries[is_largest_only][kind];
        queries = make_queries((u32)s.chunks.size(), s.largest_chunk_index, is_largest_only, box_sides[kind], rng);
        u64 num_hits = 0;
        for (const auto& q : queries)
        {
            num_hits += check_query(s.chunks[q.chunk_index], q);
            check_fractional_query(s.chunks[q.chunk_index], q, rng);
        }
        s.num_hits[is_largest_only][kind] = num_hits;
    }
}

void print_entry_count_histogram(StringView scene_name, StringView tree_name, ArrayView<u32> counts)
{
    std::sort(counts.begin(), counts.end());
    auto dbg = DBG;
    dbg << "rtree" << scene_name << tree_name << "entries:chunks";
    for (u32 i = 0; i < counts.size(); )
    {
        u32 j = i;
        while (j < counts.size() && counts[j] == counts[i])
            j++;
        dbg << cat(counts[i], ":", j - i);
        i = j;
    }
}

Pointer<benchmark_scene> make_benchmark_scene(StringView name, world& w, Random::xoshiro256starstar& rng)
{
    Pointer<benchmark_scene> ret{InPlaceInit};
    const auto chunk_count = (u32)w.size();
    fm_assert(chunk_count > 0);
    auto& chunks = ret->chunks;
    chunks = Array<chunk_structures>{ValueInit, chunk_count};
    Array<u32> entry_counts{NoInit, chunk_count}, hole_entry_counts{NoInit, chunk_count};
    u32 largest = 0, n = 0;
    for (chunk& ch : w.chunks())
    {
        auto& c = chunks[n];
        c.coord = ch.coord();
        c.old_tree_8 = *ch.rtree();
        fill_structures(c, leaf_entries(c.old_tree_8));
        entry_counts[n] = (u32)c.entries.size();
        hole_entry_counts[n] = (u32)ch.hole_rtree()->Count();
        if (entry_counts[n] > entry_counts[largest])
            largest = n;
        n++;
    }
    fm_assert(n == chunk_count);
    ret->largest_chunk_index = largest;
    ret->shuffled_nodes = make_shuffled_nodes(chunks, rng);

    const auto& c = chunks[largest];
    auto& shuffled = ret->largest_chunk_shuffled_entries;
    arrayReserve(shuffled, Math::max(c.entries.size(), usize{16}));
    arrayAppend(shuffled, ArrayView<const entry>{c.entries});
    shuffle(arrayView(shuffled), rng);

    check_array_updates(c, rng);
    for (const auto& ci : chunks)
        check_edge_queries(ci, rng);
    for (u32 kind = 0; kind < query_kind_count; kind++)
        make_checked_queries(*ret, kind, rng);

    DBG << "rtree" << name << "chunks" << cat(chunk_count, ",") << "largest tree: entries" << cat(c.entries.size(), ",")
        << "nodes" << cat(c.nodes_8.size(), ",") << "height" << cat(c.nodes_8.front().m_level + 1, ",")
        << "nodes at fan-out 16:" << cat(c.nodes_16.size(), ",") << "at 32:" << c.nodes_32.size();
    print_entry_count_histogram(name, "collider tree", entry_counts);
    print_entry_count_histogram(name, "hole tree", hole_entry_counts);
    return ret;
}

std::array<Pointer<benchmark_scene>, scene_count> make_benchmark_scenes()
{
    // Worlds are destroyed only after every tree is copied, so the copies get no node a destroyed world freed.
    std::array<Optional<world>, scene_count> worlds;
    for (u32 s = 0; s < scene_count; s++)
        worlds[s] = make_scene_world((scene)s);
    std::array<Pointer<benchmark_scene>, scene_count> scenes;
    Random::xoshiro256starstar rng;
    for (u32 s = 0; s < scene_count; s++)
        if (worlds[s])
            scenes[s] = make_benchmark_scene(scene_names[s], *worlds[s], rng);
    return scenes;
}

const std::array<Pointer<benchmark_scene>, scene_count>& benchmark_scenes()
{
    // Function-local, so the copies are destroyed before the node pools.
    static const auto scenes = make_benchmark_scenes();
    return scenes;
}

// Returns null after skipping the benchmark when the scene's save file is missing.
const benchmark_scene* get_benchmark_scene(benchmark::State& state)
{
    const auto& scenes = benchmark_scenes();
    const auto s = (u32)state.range(0);
    state.SetLabel(scene_names[s]);
    if (!scenes[s])
    {
        state.SkipWithError("save file not installed");
        return nullptr;
    }
    else
        return &*scenes[s];
}

Array<entry> make_random_entries(u32 count, ArrayView<const Vector2i> sizes, Random::xoshiro256starstar& rng)
{
    Array<entry> ret{ValueInit, count};
    for (u32 i = 0; i < count; i++)
    {
        const auto size = sizes[below(rng, (u32)sizes.size())];
        Vector2i min;
        for (u32 k = 0; k < 2; k++)
            min[k] = (i32)below(rng, (u32)Math::max(chunk_size<i32> - size[k], 1)) - half_tile<i32>;
        ret[i] = {{Math::Vector2<i16>{min}, Math::Vector2<i16>{min + size}}, i};
    }
    return ret;
}

Pointer<chunks_and_queries> make_random_rects_set(u32 entry_count, ArrayView<const Vector2i> sizes,
                                                  Random::xoshiro256starstar& rng)
{
    // The arrays take 16 bytes per entry, so the chunks' arrays together take 1 MiB.
    const auto chunk_count = Math::max((1u << 16) / entry_count, 1u);
    Pointer<chunks_and_queries> ret{InPlaceInit};
    ret->chunks = Array<chunk_structures>{ValueInit, chunk_count};
    ret->largest_chunk_index = 0;
    for (u32 i = 0; i < chunk_count; i++)
    {
        auto& c = ret->chunks[i];
        const auto insertion_order = make_random_entries(entry_count, sizes, rng);
        insert_entries(c.old_tree_8, insertion_order);
        fill_structures(c, insertion_order);
        if (i < 16)
            check_edge_queries(c, rng);
    }
    for (const auto side : { box_size, whole_chunk_side })
        make_checked_queries(*ret, (u32)(std::find(std::begin(box_sides), std::end(box_sides), side) - box_sides), rng);
    const auto& c = ret->chunks.front();
    DBG << "rtree random rects: entries" << entry_count << "chunks" << cat(chunk_count, ",") << "nodes" << c.nodes_8.size()
        << "height" << c.nodes_8.front().m_level + 1;
    return ret;
}

std::array<Pointer<chunks_and_queries>, std::size(random_rect_entry_counts)> make_random_rects_sets()
{
    Array<Vector2i> sizes;
    usize size_count = 0;
    for (const auto& sc : benchmark_scenes())
        if (sc)
            for (const auto& c : sc->chunks)
                size_count += c.entries.size();
    arrayReserve(sizes, Math::max(size_count, usize{16}));
    for (const auto& sc : benchmark_scenes())
        if (sc)
            for (const auto& c : sc->chunks)
                for (const auto& e : c.entries)
                    arrayAppend(sizes, Vector2i{e.rect.size()});
    fm_assert(!sizes.isEmpty());
    std::array<Pointer<chunks_and_queries>, std::size(random_rect_entry_counts)> sets;
    Random::xoshiro256starstar rng;
    for (u32 i = 0; i < sets.size(); i++)
        sets[i] = make_random_rects_set(random_rect_entry_counts[i], sizes, rng);
    return sets;
}

const chunks_and_queries& get_random_rects_set(benchmark::State& state)
{
    // Function-local, so the copies are destroyed before the node pools.
    static const auto sets = make_random_rects_sets();
    const auto* const it = std::find(std::begin(random_rect_entry_counts), std::end(random_rect_entry_counts),
                                     (u32)state.range(0));
    fm_assert(it != std::end(random_rect_entry_counts));
    return *sets[(u32)(it - random_rect_entry_counts)];
}

u32 box_side_index(i64 side)
{
    const auto* const it = std::find(std::begin(box_sides), std::end(box_sides), (i32)side);
    fm_assert(it != std::end(box_sides));
    return (u32)(it - box_sides);
}

template<typename Search>
u64 checksum_of_hits(const Search& search)
{
    u64 checksum = 0;
    const auto count = search([&](u64 data, const auto& r) {
        checksum += data + sum_of_coord_bits(r);
        return true;
    });
    return checksum + (u64)count;
}

constexpr auto old_tree_checksum = [](const chunk_structures& c, const query& q) {
    return checksum_of_hits([&](auto&& f) { return c.old_tree_8.Search(q.min, q.max, f); });
};

template<u32 M>
constexpr auto soa_checksum = [](const chunk_structures& c, const query& q) {
    return checksum_of_hits([&](auto&& f) { return search_tree(nodes<M>(c).front(), q.rect, f); });
};

constexpr auto shuffled_nodes_checksum = [](const chunk_structures& c, const query& q) {
    return checksum_of_hits([&](auto&& f) { return search_tree(*c.shuffled_root, q.rect, f); });
};

constexpr auto stack_search_checksum = [](const chunk_structures& c, const query& q) {
    return checksum_of_hits([&](auto&& f) { return search_tree_with_stack(c.nodes_8.front(), q.rect, f); });
};

constexpr auto arrays_checksum = [](const chunk_structures& c, const query& q) {
    return checksum_of_hits([&](auto&& f) { return search_arrays(c.arrays, q.rect, f); });
};

constexpr auto blocks_checksum = [](const chunk_structures& c, const query& q) {
    return checksum_of_hits([&](auto&& f) { return search_blocks(c.blocks, q.rect, f); });
};

template<typename F>
void run_search_loop(benchmark::State& state, const chunks_and_queries& s, u32 is_largest_only, u32 kind,
                     const F& search_checksum)
{
    const auto& queries = s.queries[is_largest_only][kind];
    const auto* const q = queries.data();
    const auto* const chunks = s.chunks.data();
    const auto n = (u32)queries.size();
    fm_assert(n == query_count);
    u64 checksum = 0;
    u32 i = 0;
    for (auto _ : state)
    {
        checksum += search_checksum(chunks[q[i].chunk_index], q[i]);
        if (++i == n)
            i = 0;
    }
    benchmark::DoNotOptimize(checksum);
    state.counters["hits"] = (double)s.num_hits[is_largest_only][kind]/n;
}

template<typename F>
void bench_search(benchmark::State& state, const F& search_checksum)
{
    if (const auto* sc = get_benchmark_scene(state))
        run_search_loop(state, *sc, (u32)state.range(1), box_side_index(state.range(2)), search_checksum);
}

// Each query's index comes from the previous query's checksum, so a query waits for the previous one to end.
template<typename F>
void bench_dependent_search(benchmark::State& state, const F& search_checksum)
{
    const auto* sc = get_benchmark_scene(state);
    if (!sc)
        return;
    const auto is_largest_only = (u32)state.range(1), kind = box_side_index(state.range(2));
    const auto& queries = sc->queries[is_largest_only][kind];
    const auto* const q = queries.data();
    const auto* const chunks = sc->chunks.data();
    static_assert(query_count == 1u << 12);
    fm_assert(queries.size() == query_count);
    u64 checksum = 0;
    u32 i = 0;
    for (auto _ : state)
    {
        checksum += search_checksum(chunks[q[i].chunk_index], q[i]);
        i = (u32)((checksum + i) * 0x9e3779b97f4a7c15u >> 52);
    }
    benchmark::DoNotOptimize(checksum);
}

template<typename F>
void bench_random_rects_search(benchmark::State& state, const F& search_checksum)
{
    run_search_loop(state, get_random_rects_set(state), (u32)state.range(1), box_side_index(state.range(2)),
                    search_checksum);
}

void RTree_Search_Old(benchmark::State& state) { bench_search(state, old_tree_checksum); }
void RTree_Search_Soa(benchmark::State& state) { bench_search(state, soa_checksum<8>); }
void RTree_Search_Soa_Fanout_16(benchmark::State& state) { bench_search(state, soa_checksum<16>); }
void RTree_Search_Soa_Fanout_32(benchmark::State& state) { bench_search(state, soa_checksum<32>); }
void RTree_Search_Soa_Shuffled_Nodes(benchmark::State& state) { bench_search(state, shuffled_nodes_checksum); }
void RTree_Search_Soa_Stack(benchmark::State& state) { bench_search(state, stack_search_checksum); }
void RTree_Search_Arrays(benchmark::State& state) { bench_search(state, arrays_checksum); }
void RTree_Search_Aosoa(benchmark::State& state) { bench_search(state, blocks_checksum); }

void RTree_Search_Dependent_Old(benchmark::State& state) { bench_dependent_search(state, old_tree_checksum); }
void RTree_Search_Dependent_Soa(benchmark::State& state) { bench_dependent_search(state, soa_checksum<8>); }
void RTree_Search_Dependent_Soa_Fanout_16(benchmark::State& state) { bench_dependent_search(state, soa_checksum<16>); }
void RTree_Search_Dependent_Arrays(benchmark::State& state) { bench_dependent_search(state, arrays_checksum); }
void RTree_Search_Dependent_Aosoa(benchmark::State& state) { bench_dependent_search(state, blocks_checksum); }

void RTree_Random_Rects_Search_Old(benchmark::State& state) { bench_random_rects_search(state, old_tree_checksum); }
void RTree_Random_Rects_Search_Soa(benchmark::State& state) { bench_random_rects_search(state, soa_checksum<8>); }
void RTree_Random_Rects_Search_Soa_Fanout_16(benchmark::State& state)
{
    bench_random_rects_search(state, soa_checksum<16>);
}
void RTree_Random_Rects_Search_Soa_Fanout_32(benchmark::State& state)
{
    bench_random_rects_search(state, soa_checksum<32>);
}
void RTree_Random_Rects_Search_Arrays(benchmark::State& state) { bench_random_rects_search(state, arrays_checksum); }
void RTree_Random_Rects_Search_Aosoa(benchmark::State& state) { bench_random_rects_search(state, blocks_checksum); }

std::vector<i64> box_side_args(std::initializer_list<i32> sides)
{
    return {sides.begin(), sides.end()};
}

// chunks: 0 sends each query to a random chunk, 1 sends all to the chunk with the most entries.
// box: a query's side in px at a random position, or whole_chunk_side for the chunk as Pass::Grid searches it.
void search_args(benchmark::Benchmark* b)
{
    b->ArgNames({"scene", "chunks", "box"})
     ->ArgsProduct({benchmark::CreateDenseRange(0, scene_count - 1, 1), {0, 1},
                    box_side_args({0, box_size, 128, 512, whole_chunk_side})})
     ->Unit(benchmark::kNanosecond);
}

void dependent_search_args(benchmark::Benchmark* b)
{
    b->ArgNames({"scene", "chunks", "box"})
     ->ArgsProduct({benchmark::CreateDenseRange(0, scene_count - 1, 1), {0, 1}, box_side_args({box_size})})
     ->Unit(benchmark::kNanosecond);
}

// chunks: 0 sends each query to a random chunk of 1 MiB of arrays, 1 sends all to one chunk.
void random_rects_args(benchmark::Benchmark* b)
{
    b->ArgNames({"entries", "chunks", "box"})
     ->ArgsProduct({{std::begin(random_rect_entry_counts), std::end(random_rect_entry_counts)}, {0, 1},
                    box_side_args({box_size, whole_chunk_side})})
     ->Unit(benchmark::kNanosecond);
}

BENCHMARK(RTree_Search_Old)->Apply(search_args);
BENCHMARK(RTree_Search_Soa)->Apply(search_args);
BENCHMARK(RTree_Search_Soa_Fanout_16)->Apply(search_args);
BENCHMARK(RTree_Search_Soa_Fanout_32)->Apply(search_args);
BENCHMARK(RTree_Search_Soa_Shuffled_Nodes)->Apply(search_args);
BENCHMARK(RTree_Search_Soa_Stack)->Apply(search_args);
BENCHMARK(RTree_Search_Arrays)->Apply(search_args);
BENCHMARK(RTree_Search_Aosoa)->Apply(search_args);

BENCHMARK(RTree_Search_Dependent_Old)->Apply(dependent_search_args);
BENCHMARK(RTree_Search_Dependent_Soa)->Apply(dependent_search_args);
BENCHMARK(RTree_Search_Dependent_Soa_Fanout_16)->Apply(dependent_search_args);
BENCHMARK(RTree_Search_Dependent_Arrays)->Apply(dependent_search_args);
BENCHMARK(RTree_Search_Dependent_Aosoa)->Apply(dependent_search_args);

BENCHMARK(RTree_Random_Rects_Search_Old)->Apply(random_rects_args);
BENCHMARK(RTree_Random_Rects_Search_Soa)->Apply(random_rects_args);
BENCHMARK(RTree_Random_Rects_Search_Soa_Fanout_16)->Apply(random_rects_args);
BENCHMARK(RTree_Random_Rects_Search_Soa_Fanout_32)->Apply(random_rects_args);
BENCHMARK(RTree_Random_Rects_Search_Arrays)->Apply(random_rects_args);
BENCHMARK(RTree_Random_Rects_Search_Aosoa)->Apply(random_rects_args);

void RTree_Ensure_Passability(benchmark::State& state)
{
    const auto* sc = get_benchmark_scene(state);
    if (!sc)
        return;
    auto w = make_scene_world((scene)state.range(0));
    auto& c = (*w)[sc->chunks[sc->largest_chunk_index].coord];
    for (auto _ : state)
    {
        c.mark_passability_modified();
        c.ensure_passability();
    }
}

template<u32 M>
void bench_move_old(benchmark::State& state)
{
    const auto* sc = get_benchmark_scene(state);
    if (!sc)
        return;
    auto tree = old_tree<M>(sc->chunks[sc->largest_chunk_index]);
    const auto rects = box_rects_on_circle();
    const auto point_count = (u32)rects.size();

    constexpr auto id = ~u64{};
    tree.Insert(rects[0].m_min, rects[0].m_max, id);
    u32 i = 0;
    for (auto _ : state)
    {
        const auto& from = rects[i];
        if (++i == point_count)
            i = 0;
        const auto& to = rects[i];
        const bool found = tree.Move(from.m_min, from.m_max, id, to.m_min, to.m_max, id);
        fm_assert(found);
    }
}

void RTree_Move_Old(benchmark::State& state) { bench_move_old<8>(state); }
void RTree_Move_Old_Fanout_16(benchmark::State& state) { bench_move_old<16>(state); }
void RTree_Move_Old_Fanout_32(benchmark::State& state) { bench_move_old<32>(state); }

void RTree_Move_Arrays(benchmark::State& state)
{
    const auto* sc = get_benchmark_scene(state);
    if (!sc)
        return;
    const auto& c = sc->chunks[sc->largest_chunk_index];
    const auto rects = int16_box_rects_on_circle();
    const auto point_count = (u32)rects.size();

    constexpr auto id = ~u64{};
    Array<entry> entries;
    arrayReserve(entries, Math::max(c.entries.size() + 1, usize{16}));
    arrayAppend(entries, ArrayView<const entry>{c.entries});
    arrayAppend(entries, entry{rects[0], id});
    entry_arrays arrays;
    fill_entry_arrays(arrays, entries);

    u32 i = 0;
    for (auto _ : state)
    {
        const auto& from = rects[i];
        if (++i == point_count)
            i = 0;
        const bool found = move_entry(arrays, from, id, rects[i]);
        fm_assert(found);
    }
}

template<u32 M>
void bench_refill_old(benchmark::State& state)
{
    const auto* sc = get_benchmark_scene(state);
    if (!sc)
        return;
    const auto& c = sc->chunks[sc->largest_chunk_index];
    const ArrayView<const entry> entries = state.range(1) ? sc->largest_chunk_shuffled_entries : c.entries;
    Chunk_RTree_Fanout<M> tree;
    for (auto _ : state)
    {
        tree.RemoveAll();
        insert_entries(tree, entries);
    }
}

void RTree_Refill_Old(benchmark::State& state) { bench_refill_old<8>(state); }
void RTree_Refill_Old_Fanout_16(benchmark::State& state) { bench_refill_old<16>(state); }
void RTree_Refill_Old_Fanout_32(benchmark::State& state) { bench_refill_old<32>(state); }

void RTree_Refill_Arrays(benchmark::State& state)
{
    const auto* sc = get_benchmark_scene(state);
    if (!sc)
        return;
    const auto& c = sc->chunks[sc->largest_chunk_index];
    entry_arrays arrays;
    for (auto _ : state)
    {
        fill_entry_arrays(arrays, c.entries);
        benchmark::DoNotOptimize(arrays.data);
    }
}

// Each iteration removes one of critter_count critters and inserts it at its next point on the circle.
template<u32 M>
void bench_remove_insert_old(benchmark::State& state)
{
    const auto* sc = get_benchmark_scene(state);
    if (!sc)
        return;
    auto tree = old_tree<M>(sc->chunks[sc->largest_chunk_index]);
    const auto rects = box_rects_on_circle();
    const auto point_count = (u32)rects.size();
    u32 positions[critter_count];
    for (u32 j = 0; j < critter_count; j++)
    {
        positions[j] = j * (point_count / critter_count);
        tree.Insert(rects[positions[j]].m_min, rects[positions[j]].m_max, first_critter_id + j);
    }
    u32 j = 0;
    for (auto _ : state)
    {
        const auto& from = rects[positions[j]];
        if (++positions[j] == point_count)
            positions[j] = 0;
        const auto& to = rects[positions[j]];
        tree.Remove(from.m_min, from.m_max, first_critter_id + j);
        tree.Insert(to.m_min, to.m_max, first_critter_id + j);
        if (++j == critter_count)
            j = 0;
    }
}

void RTree_Remove_Insert_Old(benchmark::State& state) { bench_remove_insert_old<8>(state); }
void RTree_Remove_Insert_Old_Fanout_16(benchmark::State& state) { bench_remove_insert_old<16>(state); }
void RTree_Remove_Insert_Old_Fanout_32(benchmark::State& state) { bench_remove_insert_old<32>(state); }

// The critters follow the chunk's entries, so each removal moves critter_count - 1 entries.
void RTree_Remove_Insert_Arrays(benchmark::State& state)
{
    const auto* sc = get_benchmark_scene(state);
    if (!sc)
        return;
    const auto& c = sc->chunks[sc->largest_chunk_index];
    const auto rects = int16_box_rects_on_circle();
    const auto point_count = (u32)rects.size();
    u32 positions[critter_count];
    Array<entry> entries;
    arrayReserve(entries, Math::max(c.entries.size() + critter_count, usize{16}));
    arrayAppend(entries, ArrayView<const entry>{c.entries});
    for (u32 j = 0; j < critter_count; j++)
    {
        positions[j] = j * (point_count / critter_count);
        arrayAppend(entries, entry{rects[positions[j]], first_critter_id + j});
    }
    entry_arrays arrays;
    fill_entry_arrays(arrays, entries);
    u32 j = 0;
    for (auto _ : state)
    {
        const auto& from = rects[positions[j]];
        if (++positions[j] == point_count)
            positions[j] = 0;
        const bool found = remove_entry(arrays, from, first_critter_id + j);
        fm_assert(found);
        insert_entry(arrays, rects[positions[j]], first_critter_id + j);
        if (++j == critter_count)
            j = 0;
    }
}

void scene_args(benchmark::Benchmark* b)
{
    b->ArgName("scene")->DenseRange(0, scene_count - 1);
}

void refill_args(benchmark::Benchmark* b)
{
    b->ArgNames({"scene", "shuffled"})->ArgsProduct({benchmark::CreateDenseRange(0, scene_count - 1, 1), {0, 1}});
}

BENCHMARK(RTree_Ensure_Passability)->Apply(scene_args)->Unit(benchmark::kMicrosecond);
BENCHMARK(RTree_Move_Old)->Apply(scene_args)->Unit(benchmark::kNanosecond);
BENCHMARK(RTree_Move_Old_Fanout_16)->Apply(scene_args)->Unit(benchmark::kNanosecond);
BENCHMARK(RTree_Move_Old_Fanout_32)->Apply(scene_args)->Unit(benchmark::kNanosecond);
BENCHMARK(RTree_Move_Arrays)->Apply(scene_args)->Unit(benchmark::kNanosecond);
BENCHMARK(RTree_Refill_Old)->Apply(refill_args)->Unit(benchmark::kMicrosecond);
BENCHMARK(RTree_Refill_Old_Fanout_16)->Apply(refill_args)->Unit(benchmark::kMicrosecond);
BENCHMARK(RTree_Refill_Old_Fanout_32)->Apply(refill_args)->Unit(benchmark::kMicrosecond);
BENCHMARK(RTree_Refill_Arrays)->Apply(scene_args)->Unit(benchmark::kNanosecond);
BENCHMARK(RTree_Remove_Insert_Old)->Apply(scene_args)->Unit(benchmark::kNanosecond);
BENCHMARK(RTree_Remove_Insert_Old_Fanout_16)->Apply(scene_args)->Unit(benchmark::kNanosecond);
BENCHMARK(RTree_Remove_Insert_Old_Fanout_32)->Apply(scene_args)->Unit(benchmark::kNanosecond);
BENCHMARK(RTree_Remove_Insert_Arrays)->Apply(scene_args)->Unit(benchmark::kNanosecond);

} // namespace

} // namespace floormat
