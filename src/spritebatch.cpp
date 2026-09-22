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
#include "compat/setenv.hpp"
#include "src/hwy.hpp"
#include <bit>
#include <cfloat>
#include <utility>
#include <cr/GrowableArray.h>
#include <mg/Mesh.h>
#include <mg/Buffer.h>

namespace floormat {

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
    GL::Buffer index_buffer_handle{NoCreate};
    // How much of this slot's index buffer holds identity indexes. A permuted upload overwrites
    // them, so it drops back to 0 and the next identity upload rewrites from the start.
    uint32_t index_uploaded = 0;
};

// Set to gather vertices into a staging array in depth order and keep the index buffer identity,
// so the two ways of applying the merged order can be A/B'd in one driver session.
bool permute_vertexes()
{
    static const bool ret = [] {
        const auto* s = getenv("FLOORMAT_PERMUTE_VERTEXES");
        return s && *s && !(s[0] == '0' && !s[1]);
    }();
    return ret;
}

template<typename T> void reserve(Array<T>& A, uint32_t size) // todo reuse this, many places naively reserve without 1.5
{
    if (arrayCapacity(A) < size)
        arrayReserve(A, (uint32_t)((float)size * 1.5f));
    arrayResize(A, NoInit, size);
}

// Sorts depths[0, n) ascending and sets indexes[i] = base + the position depths[i] came from.
// Equal depths keep their order. scratch needs n elements.
void sort_depths(float* depths, uint32_t* indexes, uint32_t base, uint32_t n, uint64_t* scratch)
{
    // Flipping the sign bit of a non-negative float and every bit of a negative one orders floats
    // as unsigned integers. The index in the low half makes equal depths sort by position.
    for (auto i = 0u; i < n; i++)
    {
        const auto u = std::bit_cast<uint32_t>(depths[i]);
        const auto key = u ^ ((uint32_t)((int32_t)u >> 31) | 0x80000000u);
        scratch[i] = (uint64_t)key << 32 | (base + i);
    }
    vqsort(scratch, n);
    for (auto i = 0u; i < n; i++)
    {
        const auto key = (uint32_t)(scratch[i] >> 32);
        depths[i] = std::bit_cast<float>(key ^ ((uint32_t)((int32_t)~key >> 31) | 0x80000000u));
        indexes[i] = (uint32_t)scratch[i];
    }
}

} // namespace

struct SpriteBatch::Impl
{
    merge_state m;

    quick_draw quick;

    Array<Quads::vertexes> vertex_buffer;
    // index_buffer stays identity for its whole life, so every slot can share one monotone
    // upload watermark into it. A permuted order goes to index_permuted instead.
    Array<Quads::indexes> index_buffer, index_permuted;

    Array<Quads::vertexes> verts;
    // Emission order until end_chunk, run-sorted order after, so the merge reads a key without
    // going through sort_indexes first. Nothing reads it by value once end_chunk has run.
    Array<float> depths;
    Array<uint32_t> starts, sort_indexes, merge_output, perm;
    Array<uint64_t> sort_keys;

    slot slots[slot_count];
    uint32_t slot_idx = 0;
    uint32_t buffer_capacity = 0;
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
    for (auto& s : impl.slots)
    {
        auto cap  = ensure_buffer_size<Quads::vertexes>(s.vertex_buffer_handle, impl.buffer_capacity, count);
        const auto cap2 = ensure_buffer_size<Quads::indexes>(s.index_buffer_handle, impl.buffer_capacity, count);
        fm_debug_assert(cap == cap2);
        s.index_uploaded = 0; // setData() orphaned the old contents
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
    auto& impl = *this->impl;
    const auto size = list.size();
    const auto first = (uint32_t)impl.verts.size();

    // Resize to the upper bound and shrink below, so the filtered branch needs no counting pass.
    reserve(impl.verts, first + size);
    reserve(impl.depths, first + size);

    const auto* const Vin = list.Vertexes.data();
    const auto* const Din = list.Depths.data();
    auto* const V = impl.verts.data() + first;
    auto* const D = impl.depths.data() + first;

    uint32_t n = 0;

    if (render_vobjs)
        for (; n < size; n++)
        {
            V[n] = Vin[n];
            D[n] = Din[n];
        }
    else
    {
        object* const* const O = list.Objects.data();
        for (auto i = 0u; i < size; i++)
        {
            const auto* obj = O[i];
            if (obj && obj->is_virtual())
                continue;
            V[n] = Vin[i];
            D[n] = Din[i];
            n++;
        }
        if (n != size)
        {
            arrayResize(impl.verts, NoInit, first + n);
            arrayResize(impl.depths, NoInit, first + n);
        }
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

    if (do_sort)
    {
        const auto n = last - first;
        reserve(S, last);
        reserve(impl.sort_keys, n);
        sort_depths(impl.depths.data() + first, S.data() + first, first, n, impl.sort_keys.data());
        impl.s_is_identity = false;
    }
    else
    {
        arrayResize(S, NoInit, last);
        auto* const Sp = S.data();
        for (auto i = first; i < last; i++)
            Sp[i] = i;
    }

    arrayAppend(impl.starts, last);
    impl.last_start = last;
}

// Sorting the zip_view directly moves 124 bytes per iter_move to order by a 4-byte key. Sort a
// permutation instead, then walk its cycles in place.
void SpriteBatch::sort_by_depth(SpriteList& list)
{
    auto& impl = *this->impl;
    const auto n = list.size();
    if (n < 2)
        return;

    reserve(impl.perm, n);
    reserve(impl.sort_keys, n);
    sort_depths(list.Depths.data(), impl.perm.data(), 0, n, impl.sort_keys.data());

    auto* const V = list.Vertexes.data();
    auto* const O = list.Objects.data();
    auto* const P = impl.perm.data();

    for (auto i = 0u; i < n; i++)
    {
        if (P[i] == i)
            continue;
        const auto v = V[i];
        auto* const o = O[i];
        auto j = i;
        for (;;)
        {
            const auto k = P[j];
            P[j] = j; // P[j] == j is also the visited mark, so no second array is needed
            if (k == i)
                break;
            V[j] = V[k];
            O[j] = O[k];
            j = k;
        }
        V[j] = v;
        O[j] = o;
    }
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
    const bool pv = permute_vertexes();
    if (pv)
        reserve(impl.vertex_buffer, size);

    // Array::operator[] is bounds-checked and no release build defines NDEBUG.
    // Pointers must be taken after every reserve() that can reallocate.
    const auto* const D = impl.depths.data();
    const auto* const Vin = impl.verts.data();
    const auto* const S = impl.sort_indexes.data();
    const auto* const Starts = impl.starts.data();
    auto* const V = pv ? impl.vertex_buffer.data() : nullptr;

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
        // sort_indexes already holds the order draw() needs.
        if (pv)
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

    if (pv)
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

    // Nothing permuted sort_indexes, so the merged order is the emission order and both the
    // staging copy and the index permutation would come out identity.
    const bool direct = !do_sort && impl.s_is_identity;
    if (!direct)
        sort_vertex_buffer(do_sort); // modifies V when the staging path is on
    ensure_allocated(size);

    auto& slot = impl.slots[impl.slot_idx];
    const bool staged = !direct && permute_vertexes();

    slot.vertex_buffer_handle.setSubData(0, ArrayView{ staged ? V.data() : impl.verts.data(), size });

    if (staged || direct)
    {
        auto& I = impl.index_buffer;
        const auto Isz = (uint32_t)I.size();
        reserve(I, size);
        for (auto i = Isz; i < size; i++)
            I[i] = Quads::quad_indexes(i);
        // quad_indexes(i) depends only on i, so already-uploaded entries never go stale
        if (size > slot.index_uploaded)
        {
            slot.index_buffer_handle.setSubData(slot.index_uploaded * sizeof(Quads::indexes),
                                                ArrayView{ I.data() + slot.index_uploaded, size - slot.index_uploaded });
            slot.index_uploaded = size;
        }
    }
    else
    {
        // Winding survives because quad_indexes(N) offsets a fixed pattern by N*vertexes_per_quad.
        auto& I = impl.index_permuted;
        reserve(I, size);
        auto* const Ip = I.data();
        const auto* const Sp = S.data();
        for (auto i = 0u; i < size; i++)
            Ip[i] = Quads::quad_indexes(Sp[i]);
        slot.index_buffer_handle.setSubData(0, ArrayView{ Ip, size });
        slot.index_uploaded = 0;
    }

    auto& mesh = slot.mesh;
    if (!mesh.id())
    {
        mesh = GL::Mesh{GL::MeshPrimitive::Triangles};
        mesh.addVertexBuffer(slot.vertex_buffer_handle, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{});
        mesh.setIndexBuffer(slot.index_buffer_handle, 0, Quads::index_gl_type);
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
