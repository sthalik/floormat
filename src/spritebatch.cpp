#include "spritebatch.hpp"
#include "src/quads.hpp"
#include "src/object.hpp"
#include "src/anim-atlas.hpp"
#include "src/point.inl"
#include "src/sprite-list.hpp"
#include "main/clickable.hpp"
#include "shaders/shader.hpp"
#include "loader/loader.hpp"
#include "src/sprite-atlas.hpp"
#include <cfloat>
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
};

struct quick_draw
{
    GL::Mesh _mesh{NoCreate};
    GL::Buffer _vertex_buffer{NoCreate}, _index_buffer{NoCreate};
};

struct draw_item
{
    Quads::vertexes vertexes;
    float depth;
};

} // namespace

struct SpriteBatch::Impl
{
    merge_state m;

    quick_draw quick;

    Array<Quads::vertexes> vertex_buffer;
    Array<Quads::indexes> index_buffer;

    Array<draw_item> data;
    Array<uint32_t> starts, sort_indexes, merge_output;

    GL::Mesh mesh{NoCreate};
    GL::Buffer vertex_buffer_handle{NoCreate}, index_buffer_handle{NoCreate};
    uint32_t buffer_capacity = 0;
    uint32_t last_start = 0;
    bool in_chunk = false;
};

SpriteBatch::SpriteBatch()
{
    auto& impl = *this->impl;
    arrayReserve(impl.index_buffer, 16);
    arrayReserve(impl.data, 16);
    arrayReserve(impl.starts, 16);
    arrayReserve(impl.sort_indexes, 16);
}

SpriteBatch::~SpriteBatch() noexcept = default;

void SpriteBatch::begin_chunk()
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);
    fm_debug_assert(impl.sort_indexes.size() == impl.data.size());
    fm_debug_assert(impl.last_start == impl.data.size());
    impl.in_chunk = true;
}

void SpriteBatch::clear()
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);
    arrayClear(impl.vertex_buffer);
    arrayClear(impl.data);
    arrayClear(impl.starts);
    arrayClear(impl.sort_indexes);
    arrayClear(impl.merge_output);
    arrayClear(impl.m.runs);
    arrayClear(impl.m.tree);
    //arrayClear(impl.index_buffer);
    impl.last_start = 0;
    impl.in_chunk = false;
}

void SpriteBatch::ensure_allocated(uint32_t count)
{
    auto& impl = *this->impl;
    auto cap  = ensure_buffer_size<Quads::vertexes>(impl.vertex_buffer_handle, impl.buffer_capacity, count);
    auto cap2 = ensure_buffer_size<Quads::indexes>(impl.index_buffer_handle, impl.buffer_capacity, count);
    fm_debug_assert(cap == cap2);
    impl.buffer_capacity = cap;
}

void SpriteBatch::emit(const Quads::vertexes& vertexes, float depth)
{
    auto& impl = *this->impl;
    fm_assert(impl.in_chunk);
    arrayAppend(impl.data, NoInit, 1);
    auto& item = impl.data.back();
    item.vertexes = vertexes;
    item.depth = depth;
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

    const auto& A = impl.data;
    const auto first = impl.last_start;
    const auto last = (uint32_t)A.size();
    auto& S = impl.sort_indexes;

    if (first == last)
        return;

    fm_debug_assert(S.size() == first);
    arrayResize(S, NoInit, last);

    for (auto i = first; i < last; i++)
        S.data()[i] = i;

    if (do_sort)
        ranges::sort(S.slice(first, last), [&A = std::as_const(impl.data)](auto i, auto j) { return A[i].depth < A[j].depth; });

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
    const auto& D = impl.data;
    const auto size = (uint32_t)D.size();
    auto& V = impl.vertex_buffer;
    auto& M = impl.merge_output;
    const auto& S = impl.sort_indexes;
    const auto& Starts = impl.starts;
    auto& runs = impl.m.runs;
    auto& tree = impl.m.tree;

    fm_assert(V.isEmpty());
    fm_debug_assert(M.isEmpty());
    fm_debug_assert(runs.isEmpty());
    fm_debug_assert(tree.isEmpty());
    reserve(V, size);

    const auto k = (uint32_t)Starts.size() - 1; // number of runs
    if (!do_sort || k <= 1)
    {
        // sort skipped (depth-buffered opaque pass), single chunk, or empty —
        // copy vertices in input order.
        for (auto i = 0u; i < size; i++)
            V[i] = D[S[i]].vertexes;
        return;
    }

    reserve(M, size);
    reserve(runs, k);
    reserve(tree, k);

    // --- k-way merge via loser tree ---

    for (auto i = 0u; i < k; i++)
        runs[i] = {Starts[i], Starts[i + 1]};

    const uint32_t sentinel = k;
    auto depth_of = [&](uint32_t r) -> float
    {
        if (r >= k) return -FLT_MAX; // sentinel always wins
        if (runs[r].pos >= runs[r].end) return FLT_MAX; // exhausted run always loses
        return D[S[runs[r].pos]].depth;
    };

    // build tree: insert runs back to front
    for (auto i = 0u; i < k; i++)
        tree[i] = sentinel;
    for (auto i = k - 1; i != (uint32_t)-1; i--)
    {
        uint32_t winner = i;
        for (uint32_t p = (k + i) / 2; p > 0; p /= 2)
            if (depth_of(winner) > depth_of(tree[p]))
                std::swap(winner, tree[p]);
        tree[0] = winner;
    }

    // extract in sorted order
    for (auto i = 0u; i < size; i++)
    {
        auto w = tree[0];
        fm_debug_assert(w < k && runs[w].pos < runs[w].end);
        M[i] = S[runs[w].pos];
        runs[w].pos++;

        // replay from leaf w
        uint32_t winner = w;
        for (uint32_t p = (k + w) / 2; p > 0; p /= 2)
            if (depth_of(winner) > depth_of(tree[p]))
                std::swap(winner, tree[p]);
        tree[0] = winner;
    }

    // write vertices in merged order
    for (auto i = 0u; i < size; i++)
        V[i] = D[M[i]].vertexes;

    // swap so draw() reads merged order from sort_indexes
    std::swap(impl.sort_indexes, impl.merge_output);

#if !defined FM_NO_DEBUG2 && !defined __FAST_MATH__ /* hack */
    for (auto i = 1u; i < size; i++)
    {
        const auto &a = S[i-1], &b = S[i];
        const auto ad = D[a].depth, bd = D[b].depth;
        fm_assert(ad <= bd);
    }
#endif
}

void SpriteBatch::draw(tile_shader& shader, bool do_sort)
{
    auto& impl = *this->impl;
    fm_assert(!impl.in_chunk);
    const auto& D = impl.data;
    const auto size = (uint32_t)D.size();

    if (size == 0)
        return;

    const auto& S = impl.sort_indexes;
    auto& V = impl.vertex_buffer;
    fm_debug_assert(V.isEmpty());
    fm_debug_assert(size == S.size());
    fm_debug_assert(!size == impl.starts.isEmpty());
    fm_debug_assert(impl.last_start == size);
    fm_debug_assert(impl.merge_output.isEmpty());
    arrayAppend(impl.starts, impl.last_start);

    sort_vertex_buffer(do_sort); // modifies V
    ensure_allocated(size);
    impl.vertex_buffer_handle.setSubData(0, ArrayView{ V.data(), size });

    auto& I = impl.index_buffer;
    const auto Isz = (uint32_t)I.size();
    reserve(I, size);
    for (auto i = Isz; i < size; i++)
        I[i] = Quads::quad_indexes(i);

    impl.index_buffer_handle.setSubData(0, ArrayView{ I.data(), size });

    auto& mesh = impl.mesh;
    if (!mesh.id())
    {
        mesh = GL::Mesh{GL::MeshPrimitive::Triangles};
        mesh.addVertexBuffer(impl.vertex_buffer_handle, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{});
        mesh.setIndexBuffer(impl.index_buffer_handle, 0, GL::MeshIndexType::UnsignedShort);
    }
    mesh.setCount(6 * (Int)V.size());
    fm_assert(mesh.isIndexed());

    shader.draw(loader.atlas().texture(), mesh);

    clear();
}

void SpriteBatch::emit_quick(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, const Vector3& center, const Quads::depths& depth)
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
        mesh.setIndexBuffer(quick._index_buffer, 0, GL::MeshIndexType::UnsignedShort);
        mesh.setCount(6);
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
