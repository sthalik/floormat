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
    Array<run> runs;
    Array<uint32_t> tree;
    Array<float> head; // FLT_MAX once the run is exhausted
};

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

} // namespace

struct SpriteBatch::Impl
{
    merge_state m;

    quick_draw quick;

    Array<Quads::vertexes> vertex_buffer;
    Array<Quads::indexes> index_buffer;

    Array<Quads::vertexes> verts;
    Array<float> depths;
    Array<uint32_t> starts, sort_indexes, merge_output;

    slot slots[slot_count];
    GL::Buffer index_buffer_handle{NoCreate};
    uint32_t slot_idx = 0;
    uint32_t buffer_capacity = 0;
    uint32_t index_uploaded = 0;
    uint32_t last_start = 0;
    bool in_chunk = false;
    // Cleared by end_chunk(true), the only thing that permutes sort_indexes.
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

    if (first == last)
        return;

    fm_debug_assert(S.size() == first);
    arrayResize(S, NoInit, last);

    for (auto i = first; i < last; i++)
        S.data()[i] = i;

    if (do_sort)
    {
        ranges::sort(S.slice(first, last), [&A = std::as_const(impl.depths)](auto i, auto j) { return A[i] < A[j]; });
        impl.s_is_identity = false;
    }

    arrayAppend(impl.starts, first);
    impl.last_start = last;
}

template<typename T> void reserve(Array<T>& A, uint32_t size) // todo reuse this, many places naively reserve without 1.5
{
    if (arrayCapacity(A) < size)
        arrayReserve(A, (uint32_t)((float)size * 1.5f));
    arrayResize(A, NoInit, size);
}

void SpriteBatch::sort_vertex_buffer(bool do_sort)
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);
    // One-past-the-end terminator, so the run loop below reads bounds without a branch.
    arrayAppend(impl.starts, impl.last_start);

    const auto& Dep = impl.depths;
    const auto& Vin = impl.verts;
    const auto size = (uint32_t)Vin.size();
    auto& V = impl.vertex_buffer;
    auto& M = impl.merge_output;
    const auto& S = impl.sort_indexes;
    const auto& Starts = impl.starts;
    auto& runs = impl.m.runs;
    auto& tree = impl.m.tree;
    auto& head = impl.m.head;

    fm_assert(V.isEmpty());
    fm_debug_assert(M.isEmpty());
    fm_debug_assert(runs.isEmpty());
    fm_debug_assert(tree.isEmpty());
    fm_debug_assert(head.isEmpty());
    reserve(V, size);

    const auto k = (uint32_t)Starts.size() - 1; // number of runs
    if (!do_sort || k <= 1)
    {
        // sort skipped (depth-buffered opaque pass), single chunk, or empty —
        // copy vertices in input order.
        for (auto i = 0u; i < size; i++)
            V[i] = Vin[S[i]];
        return;
    }

    reserve(M, size);
    reserve(runs, k);
    reserve(tree, k);
    reserve(head, k);

    // --- k-way merge via loser tree ---

    // Runs are never empty: end_chunk() returns before appending to starts when first == last.
    for (auto i = 0u; i < k; i++)
    {
        runs[i] = {Starts[i], Starts[i + 1]};
        head[i] = Dep[S[Starts[i]]];
    }

    const uint32_t sentinel = k;
    auto depth_of = [&](uint32_t r) -> float { return r >= k ? -FLT_MAX : head[r]; };

    // build tree: insert runs back to front
    for (auto i = 0u; i < k; i++)
        tree[i] = sentinel;
    for (auto i = k - 1; i != (uint32_t)-1; i--)
    {
        uint32_t winner = i;
        float wd = depth_of(i);
        for (uint32_t p = (k + i) / 2; p > 0; p /= 2)
        {
            const float pd = depth_of(tree[p]);
            if (wd > pd)
            {
                std::swap(winner, tree[p]);
                wd = pd;
            }
        }
        tree[0] = winner;
    }

    // Runner-up key. Valid only while tree[0] is unchanged, so a replay that moves the
    // winner resets it. The seed forces a full replay on iteration 0.
    float second = -FLT_MAX;

    // extract in sorted order
    for (auto i = 0u; i < size; i++)
    {
        const auto w = tree[0];
        fm_debug_assert(w < k && runs[w].pos < runs[w].end);
        M[i] = S[runs[w].pos];
        runs[w].pos++;
        head[w] = runs[w].pos < runs[w].end ? Dep[S[runs[w].pos]] : FLT_MAX;

        // Still the winner, so the tree, tree[0] and `second` are all unchanged.
        if (head[w] <= second)
            continue;

        // replay from leaf w
        uint32_t winner = w;
        float wd = head[w], lo = FLT_MAX;
        for (uint32_t p = (k + w) / 2; p > 0; p /= 2)
        {
            const float pd = depth_of(tree[p]);
            if (wd > pd)
            {
                std::swap(winner, tree[p]);
                wd = pd;
            }
            else if (pd < lo)
                lo = pd;
        }
        tree[0] = winner;
        // With no swap the else branch ran at every level, so lo is the min over all path
        // losers. Once swapped it is a partial min over the wrong path.
        second = winner == w ? lo : -FLT_MAX;
    }

    // write vertices in merged order
    for (auto i = 0u; i < size; i++)
        V[i] = Vin[M[i]];

    // swap so draw() reads merged order from sort_indexes
    std::swap(impl.sort_indexes, impl.merge_output);

#if !defined FM_NO_DEBUG2 && !defined __FAST_MATH__ /* hack */
    for (auto i = 1u; i < size; i++)
    {
        const auto &a = S[i-1], &b = S[i];
        const auto ad = Dep[a], bd = Dep[b];
        fm_assert(ad <= bd);
    }
#endif
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
    fm_debug_assert(!size == impl.starts.isEmpty());
    fm_debug_assert(impl.last_start == size);
    fm_debug_assert(impl.merge_output.isEmpty());

    // Nothing permuted sort_indexes, so the staging copy would reproduce impl.verts
    // exactly. Upload from it and skip both the identity gather and the V allocation.
    const bool direct = !do_sort && impl.s_is_identity;
    if (!direct)
        sort_vertex_buffer(do_sort); // modifies V
#ifndef FM_NO_DEBUG2
    else
        for (auto i = 0u; i < size; i++)
            fm_assert(S[i] == i);
#endif
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
    Quads::vertexes vertexes;
    for (auto i = 0uz; i < 4; i++)
        vertexes[i] = { pos[i], uv3[i], depth[i] };
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
