#include "spritebatch.hpp"
#include "src/quads-gl.hpp"
#include "src/object.hpp"
#include "src/anim-atlas.hpp"
#include "src/point.inl"
#include "src/sprite-list.hpp"
#include "main/clickable.hpp"
#include "shaders/shader.hpp"
#include "loader/loader.hpp"
#include "src/sprite-atlas.hpp"
#include <cfloat>
#include <utility>
#include <ranges>
#include <algorithm>
#include <cr/GrowableArray.h>
#include <mg/Mesh.h>
#include <mg/Buffer.h>

namespace floormat {

namespace ranges = std::ranges;

namespace {

template<typename T>
uint32_t ensure_buffer_size(GL::Buffer& buf, uint32_t capacity, uint32_t count)
{
    if (count <= capacity) [[likely]]
        return capacity;
    constexpr auto factor = 1.5f;
    auto new_cap = uint32_t((float)count * factor);
    if (!buf.id())
        buf = GL::Buffer{};
    buf.setData({nullptr, new_cap * sizeof(T)}, GL::BufferUsage::DynamicDraw);
    return new_cap;
}

struct merge_state
{
    struct run { uint32_t pos, end; };
    // The loser's key rides with its index: one load per replay level. Correct because a run's
    // head changes only while it is tree[0], never while it sits as a loser.
    struct node { uint32_t run; float d; };
    Array<run> runs;
    Array<node> tree;
    Array<float> head; // FLT_MAX once the run is exhausted
};

// Sort key and payload packed, so the comparator reads the key inline rather than gathering
// depths[i] on every one of the N log N comparisons.
struct sort_key { float d; uint32_t i; };

struct quick_draw
{
    GL::Mesh _mesh{NoCreate};
    GL::Buffer _vertex_buffer{NoCreate}, _index_buffer{NoCreate};
};

constexpr uint32_t slot_count = 3;

struct slot
{
    GL::Mesh mesh{NoCreate};
    GL::Buffer vertex_buffer_handle{NoCreate};
};

template<typename T> void reserve(Array<T>& A, uint32_t size) // todo reuse this, many places naively reserve without 1.5
{
    if (arrayCapacity(A) < size)
        arrayReserve(A, (uint32_t)((float)size * 1.5f));
    arrayResize(A, NoInit, size);
}

} // namespace

struct SpriteBatch::Impl
{
    merge_state m;

    quick_draw quick;

    Array<Quads::vertexes> vertex_buffer;
    Array<Quads::indexes> index_buffer;

    Array<Quads::vertexes> verts;
    // Emission order until end_chunk, run-sorted order after, so the merge reads a key without
    // going through sort_indexes first. Nothing reads it by value once end_chunk has run.
    Array<float> depths;
    Array<uint32_t> starts, sort_indexes, merge_output;
    Array<sort_key> sort_keys;

    slot slots[slot_count];
    GL::Buffer index_buffer_handle{NoCreate};
    uint32_t slot_idx = 0;
    uint32_t buffer_capacity = 0;
    uint32_t index_uploaded = 0;
    uint32_t last_start = 0;
    bool in_chunk = false;
    // Cleared by both things that permute sort_indexes: end_chunk(true) and the merge.
    bool s_is_identity = true;
};

ArrayView<const uint32_t> SpriteBatch::merged_order() const { return impl->sort_indexes; }

SpriteBatch::SpriteBatch()
{
    auto& impl = *this->impl;
    arrayReserve(impl.index_buffer, 16);
    arrayReserve(impl.verts, 16);
    arrayReserve(impl.depths, 16);
    arrayReserve(impl.starts, 16);
    arrayAppend(impl.starts, 0u);
    arrayReserve(impl.sort_indexes, 16);
}

SpriteBatch::~SpriteBatch() noexcept = default;

void SpriteBatch::begin_chunk()
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);
    fm_debug_assert(impl.sort_indexes.size() == impl.verts.size());
    fm_debug_assert(impl.last_start == impl.verts.size());
    impl.in_chunk = true;
}

void SpriteBatch::clear()
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);
    arrayClear(impl.vertex_buffer);
    arrayClear(impl.verts);
    arrayClear(impl.depths);
    arrayClear(impl.starts);
    arrayAppend(impl.starts, 0u); // leading bound, so end_chunk appends only run ends
    arrayClear(impl.sort_indexes);
    arrayClear(impl.merge_output);
    arrayClear(impl.m.runs);
    arrayClear(impl.m.tree);
    arrayClear(impl.m.head);
    //arrayClear(impl.index_buffer);
    impl.last_start = 0;
    impl.in_chunk = false;
    impl.s_is_identity = true;
}

void SpriteBatch::ensure_allocated(uint32_t count)
{
    auto& impl = *this->impl;
    if (count <= impl.buffer_capacity) [[likely]]
        return;
    uint32_t new_cap = 0;
    const auto cap2 = ensure_buffer_size<Quads::indexes>(impl.index_buffer_handle, impl.buffer_capacity, count);
    impl.index_uploaded = 0; // setData() orphaned the old contents
    for (auto& s : impl.slots)
    {
        auto cap  = ensure_buffer_size<Quads::vertexes>(s.vertex_buffer_handle, impl.buffer_capacity, count);
        fm_debug_assert(cap == cap2);
        new_cap = cap;
    }
    impl.buffer_capacity = new_cap;
}

void SpriteBatch::emit(const Quads::vertexes& vertexes, float depth)
{
    auto& impl = *this->impl;
    fm_assert(impl.in_chunk);
    arrayAppend(impl.verts, NoInit, 1);
    arrayAppend(impl.depths, NoInit, 1);
    impl.verts.back() = vertexes;
    impl.depths.back() = depth;
}

void SpriteBatch::emit(SpriteList& list, bool render_vobjs)
{
    begin_chunk();
    const auto size = list.size();
    for (auto i = 0u; i < size; i++)
    {
        const auto& v = list.Vertexes[i];
        const auto& d = list.Depths[i];
        auto* obj = list.Objects[i];
        if (obj && !render_vobjs && obj->is_virtual())
            continue;
        emit(v, d);
    }
    end_chunk(false);
}

void SpriteBatch::end_chunk(bool do_sort)
{
    auto& impl = *this->impl;
    fm_assert(impl.in_chunk);
    impl.in_chunk = false;

    const auto first = impl.last_start;
    const auto last = (uint32_t)impl.verts.size();
    auto& S = impl.sort_indexes;

    if (first == last) [[unlikely]]
        return;

    fm_debug_assert(S.size() == first);
    arrayResize(S, NoInit, last);

    for (auto i = first; i < last; i++)
        S.data()[i] = i;

    const auto n = last - first;
    auto* const D = impl.depths.data();

    if (do_sort)
    {
        reserve(impl.sort_keys, n);
        auto* const K = impl.sort_keys.data();
        // K holds every key before the write loop starts, so permuting D in place is safe.
        for (auto i = 0u; i < n; i++)
            K[i] = {D[first + i], first + i};
        ranges::sort(K, K + n, [](const sort_key& a, const sort_key& b) { return a.d < b.d; });
        auto* const Sp = S.data();
        for (auto i = 0u; i < n; i++)
        {
            Sp[first + i] = K[i].i;
            D[first + i] = K[i].d;
        }
        impl.s_is_identity = false;
    }

    arrayAppend(impl.starts, last);
    impl.last_start = last;
}

void SpriteBatch::sort_vertex_buffer(bool do_sort)
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);

    const auto size = (uint32_t)impl.verts.size();
    const auto k = (uint32_t)impl.starts.size() - 1; // number of runs

    fm_assert(impl.vertex_buffer.isEmpty());
    fm_debug_assert(impl.merge_output.isEmpty());
    fm_debug_assert(impl.m.runs.isEmpty());
    fm_debug_assert(impl.m.tree.isEmpty());
    fm_debug_assert(impl.m.head.isEmpty());
    reserve(impl.vertex_buffer, size);

    // Array::operator[] is bounds-checked and no release build defines NDEBUG.
    // Pointers must be taken after every reserve() that can reallocate.
    const auto* const D = impl.depths.data();
    const auto* const Vin = impl.verts.data();
    const auto* const S = impl.sort_indexes.data();
    const auto* const Starts = impl.starts.data();
    auto* const V = impl.vertex_buffer.data();

#ifndef FM_NO_DEBUG3
    // end_chunk(false) trusts the caller to have sorted the run. The only such caller feeding
    // a sorted batch is chunk::scenery_static_mesh, sorted under `if (modify_static)`.
    if (do_sort)
        for (auto r = 0u; r < k; r++)
            for (auto i = Starts[r] + 1; i < Starts[r + 1]; i++)
                fm_assert(D[i-1] <= D[i]);
#endif

    if (!do_sort || k <= 1)
    {
        // sort skipped (depth-buffered opaque pass), single chunk, or empty —
        // copy vertices in input order.
        for (auto i = 0u; i < size; i++)
            V[i] = Vin[S[i]];
        return;
    }

    reserve(impl.merge_output, size);
    reserve(impl.m.runs, k);
    reserve(impl.m.tree, k);
    reserve(impl.m.head, k + 1);

    auto* const M = impl.merge_output.data();
    auto* const runs = impl.m.runs.data();
    auto* const tree = impl.m.tree.data();
    auto* const head = impl.m.head.data();

    // --- k-way merge via loser tree ---

    // Runs are never empty: end_chunk() returns before appending to starts when first == last.
    for (auto i = 0u; i < k; i++)
    {
        runs[i] = {Starts[i], Starts[i + 1]};
        head[i] = D[Starts[i]];
    }

    const uint32_t sentinel = k;
    head[sentinel] = -FLT_MAX; // wins every comparison, so the build displaces it out of tree[]

    // build tree: insert runs back to front
    for (auto i = 0u; i < k; i++)
        tree[i] = {sentinel, -FLT_MAX};
    for (auto i = k - 1; i != (uint32_t)-1; i--)
    {
        merge_state::node cur{i, head[i]};
        for (uint32_t p = (k + i) / 2; p > 0; p /= 2)
            if (cur.d > tree[p].d)
                std::swap(cur, tree[p]);
        tree[0] = cur;
    }

    // Runner-up key. Valid only while tree[0] is unchanged, so a replay that moves the
    // winner resets it. The seed forces a full replay on iteration 0.
    float second = -FLT_MAX;

    // extract in sorted order
    for (auto i = 0u; i < size; i++)
    {
        const auto w = tree[0].run;
        auto& rw = runs[w];
        M[i] = S[rw.pos];
        rw.pos++;
        head[w] = rw.pos < rw.end ? D[rw.pos] : FLT_MAX;

        // Still the winner, so the tree, tree[0] and `second` are all unchanged.
        if (head[w] <= second)
            continue;

        // replay from leaf w
        merge_state::node cur{w, head[w]};
        float lo = FLT_MAX;
        for (uint32_t p = (k + w) / 2; p > 0; p /= 2)
        {
            if (cur.d > tree[p].d)
                std::swap(cur, tree[p]);
            else if (tree[p].d < lo)
                lo = tree[p].d;
        }
        tree[0] = cur;
        // With no swap the else branch ran at every level, so lo is the min over all path
        // losers. Once swapped it is a partial min over the wrong path.
        second = cur.run == w ? lo : -FLT_MAX;
    }

    // write vertices in merged order
    for (auto i = 0u; i < size; i++)
        V[i] = Vin[M[i]];

    // swap so draw() reads merged order from sort_indexes
    std::swap(impl.sort_indexes, impl.merge_output);
    impl.s_is_identity = false;
}

void SpriteBatch::unsort_vertex_buffer()
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);
    arrayClear(impl.vertex_buffer);
    // The early-out fills V and returns without swapping, so it leaves merge_output empty.
    if (!impl.merge_output.isEmpty())
    {
        std::swap(impl.sort_indexes, impl.merge_output);
        arrayClear(impl.merge_output);
    }
    arrayClear(impl.m.runs);
    arrayClear(impl.m.tree);
    arrayClear(impl.m.head);
}

void SpriteBatch::draw(tile_shader& shader, bool do_sort)
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);
    const auto size = (uint32_t)impl.verts.size();

    if (size == 0)
        return;

    // raise the per-draw cap by widening Quads::index_type in src/quads.hpp.
    fm_assert(size <= Quads::max_quads_per_buffer);

    const auto& S = impl.sort_indexes;
    auto& V = impl.vertex_buffer;
    fm_debug_assert(V.isEmpty());
    fm_debug_assert(size == S.size());
    fm_debug_assert(size == impl.depths.size());
    fm_debug_assert(impl.starts.size() > 1);
    fm_debug_assert(impl.last_start == size);
    fm_debug_assert(impl.merge_output.isEmpty());

    // Nothing permuted sort_indexes, so the staging copy would reproduce impl.verts
    // exactly. Upload from it and skip both the identity gather and the V allocation.
    const bool direct = !do_sort && impl.s_is_identity;
    if (!direct)
        sort_vertex_buffer(do_sort); // modifies V
    ensure_allocated(size);

    auto& slot = impl.slots[impl.slot_idx];

    slot.vertex_buffer_handle.setSubData(0, ArrayView{ direct ? impl.verts.data() : V.data(), size });

    auto& I = impl.index_buffer;
    const auto Isz = (uint32_t)I.size();
    reserve(I, size);
    for (auto i = Isz; i < size; i++)
        I[i] = Quads::quad_indexes(i);
    // quad_indexes(i) depends only on i, so already-uploaded entries never go stale
    if (size > impl.index_uploaded)
    {
        impl.index_buffer_handle.setSubData(impl.index_uploaded * sizeof(Quads::indexes),
                                            ArrayView{ I.data() + impl.index_uploaded, size - impl.index_uploaded });
        impl.index_uploaded = size;
    }

    auto& mesh = slot.mesh;
    if (!mesh.id())
    {
        mesh = GL::Mesh{GL::MeshPrimitive::Triangles};
        mesh.addVertexBuffer(slot.vertex_buffer_handle, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{});
        mesh.setIndexBuffer(impl.index_buffer_handle, 0, Quads::index_gl_type);
    }
    mesh.setCount((Int)(Quads::indexes_per_quad * size));
    fm_assert(mesh.isIndexed());

    shader.draw(loader.atlas().texture(), mesh);

    impl.slot_idx = (impl.slot_idx + 1) % slot_count;
    clear();
}

void SpriteBatch::emit_quick(tile_shader& shader, const anim_atlas& atlas, rotation r, size_t frame,
                             const Vector3& center, const Quads::depths& depth)
{
    auto& impl = *this->impl;
    const auto pos = atlas.frame_quad(center, r, frame);
    const auto& g = atlas.group(r);
    const auto* sp = g.sprites[frame];
    fm_assert(sp);
    const auto uv3 = loader.atlas().texcoords_for(sprite{sp}, !g.mirror_from.isEmpty());
    const auto vertexes = Quads::make_vertexes(pos, uv3, depth);
    const auto indexes = Quads::quad_indexes(0);
    auto& quick = impl.quick;
    auto& mesh = quick._mesh;

    if (!quick._vertex_buffer.id())
        quick._vertex_buffer = GL::Buffer{{nullptr, sizeof vertexes}, GL::BufferUsage::DynamicDraw};
    quick._vertex_buffer.setSubData(0, {&vertexes, 1});

    if (!quick._index_buffer.id())
        quick._index_buffer = GL::Buffer{{nullptr, sizeof indexes}, GL::BufferUsage::DynamicDraw};
    quick._index_buffer.setSubData(0, {&indexes, 1});

    if (!mesh.id())
    {
        mesh = GL::Mesh{GL::MeshPrimitive::Triangles};
        mesh.addVertexBuffer(quick._vertex_buffer, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{});
        mesh.setIndexBuffer(quick._index_buffer, 0, Quads::index_gl_type);
        mesh.setCount((Int)Quads::indexes_per_quad);
        fm_assert(mesh.isIndexed());
    }
    shader.draw(loader.atlas().texture(), quick._mesh);
}

void SpriteBatch::add_clickable(object* obj, const tile_shader& shader, Vector2i win_size, Array<clickable>& array)
{
    const auto& s = *obj;
    const auto& a = *s.atlas;
    const auto& g = a.group(s.r);
    const auto& f = a.frame(s.r, s.frame);
    const Vector2i offset((Vector2(shader.camera_offset()) + Vector2(win_size)*.5f)
                          + shader.project(Vector3(s.position()) + Vector3(g.offset)) - Vector2(f.ground));
    if (offset < win_size && offset + Vector2i(f.size) >= Vector2i())
    {
        arrayAppend(array, NoInit, 1);
        array.back() =
            clickable {
                .src = {f.offset, f.offset + f.size},
                .dest = {offset, offset + Vector2i(f.size)},
                .bitmask = a.bitmask(),
                .e = obj,
                .stride = a.info().pixel_size[0],
                .mirrored = !g.mirror_from.isEmpty(),
            };
    }
}

} // namespace floormat
