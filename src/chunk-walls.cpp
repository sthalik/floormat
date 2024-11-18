#include "chunk.hpp"
#include "tile-bbox.hpp"
#include "quads.hpp"
#include "wall-atlas.hpp"
#include "tile-bbox.hpp"
#include "compat/unroll.hpp"
#include "RTree-search.hpp"
#include "shaders/shader.hpp"
#include <cr/ArrayViewStl.h>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <concepts>
#include <algorithm>
#include <ranges>

namespace floormat {

namespace ranges = std::ranges;

void chunk::ensure_alloc_walls()
{
    if (!_walls) [[unlikely]]
        _walls = Pointer<wall_stuff>{InPlaceInit};
}

wall_atlas* chunk::wall_atlas_at(size_t i) const noexcept
{
    if (!_walls) [[unlikely]]
        return {};
    fm_debug_assert(i < TILE_COUNT*2);
    return _walls->atlases[i].get();
}

namespace {

using Wall::Group;
using Wall::Group_;
using Wall::Direction_;
using Wall::Frame;

template<typename T> using Vec2_ = VectorTypeFor<2, T>;
template<typename T> using Vec3_ = VectorTypeFor<3, T>;

template<typename F, size_t N>
struct minmax_v
{
    VectorTypeFor<N, F> min, max;
};

struct quad_table_entry
{
    bool x : 1, y : 1, z : 1, dmx :1 = false, dmy : 1 = false, _fuzz : 3 = false;
};
static_assert(sizeof(quad_table_entry) == sizeof(uint8_t));

template<bool IsWest>
constexpr std::array<quad_table_entry, 4> make_quad_table_entry(Group_ G)
{
    fm_assert(G < Group_::COUNT);

    constexpr bool x0 = false, x1 = true,
                   y0 = false, y1 = true,
                   z0 = false, z1 = true;
    constexpr bool t = true, f = false;

    switch (G)
    {
    using enum Group_;
    case COUNT: std::unreachable();
    case wall:
        if (!IsWest)
            return {{
                { x1, y0, z0 },
                { x1, y0, z1 },
                { x0, y0, z0 },
                { x0, y0, z1 },
            }};
        else
            return {{
                { x0, y0, z0 },
                { x0, y0, z1 },
                { x0, y1, z0 },
                { x0, y1, z1 },
            }};
    case side:
        if (!IsWest)
            return {{
                { x1, y0, z0, f, t },
                { x1, y0, z1, f, t },
                { x1, y0, z0       },
                { x1, y0, z1       },
            }};
        else
            return {{
                { x0, y1, z0       },
                { x0, y1, z1       },
                { x0, y1, z0, t, f },
                { x0, y1, z1, t, f },
            }};
    case top:
        if (!IsWest)
            return {{
                { x0, y0, z1, f, t },
                { x1, y0, z1, f, t },
                { x0, y0, z1       },
                { x1, y0, z1       },
            }};
        else
            return {{
                { x0, y0, z1       },
                { x0, y1, z1       },
                { x0, y0, z1, t, f },
                { x0, y1, z1, t, f },
            }};
    case corner:
        if (!IsWest)
            return {{
                { x0, y0, z0       },
                { x0, y0, z1       },
                { x0, y0, z0, t, f },
                { x0, y0, z1, t, f },
            }};
        else
            return {{
                { x0, y0, z0, f, t },
                { x0, y0, z1, f, t },
                { x0, y0, z0       },
                { x0, y0, z1       },
            }};
    }
    std::unreachable();
}

template<Group_ G, bool IsWest, typename F = float>
constexpr auto get_quadʹ(minmax_v<F, 3> bounds, F d)
{
    F x[2] { bounds.min.x(), bounds.max.x() },
      y[2] { bounds.min.y(), bounds.max.y() },
      z[2] { bounds.min.z(), bounds.max.z() },
      dmx[2] { 0, d },
      dmy[2] { 0, d };

    std::array<Vec3_<F>, 4> array = {};

    unroll<static_array_size<decltype(array)>>([&]<typename Index>(Index) {
        constexpr size_t i = Index::value;
        constexpr auto table = make_quad_table_entry<IsWest>(G);
        constexpr auto e = table[i];
        array.data()[i] = { x[e.x] - dmx[e.dmx], y[e.y] - dmy[e.dmy], z[e.z], };
    });

    return array;
}

template<Group_ G, bool IsWest, typename F = float>
constexpr CORRADE_ALWAYS_INLINE auto get_quad(F d)
{
    constexpr auto half_tile = Vec2_<F>(TILE_SIZE2*.5f);
    constexpr auto X = half_tile.x(), Y = half_tile.y(), Z = F(TILE_SIZE.z());

    return get_quadʹ<G, IsWest, F>({ { -X, -Y, 0, }, { X, Y, Z, }, }, d);
}

template<bool IsWest>
CutResult<Int>::rect get_wall_rect(local_coords tile)
{
    constexpr auto nʹ = wall_north<Int>(0, 0), wʹ = wall_west<Int>(0, 0);
    constexpr auto t0 = !IsWest ? nʹ.first() : wʹ.first(),
                   t1 = !IsWest ? nʹ.second() : wʹ.second();
    const auto offset = Vector2i{tile} * iTILE_SIZE2;
    const auto min = offset + t0, max = offset + t1;
    return { min, max };
}

template<bool IsWest, std::invocable<Vector2, Vector2> F>
void cut_holes_in_wall(chunk& c, local_coords tile, Vector2i min, Vector2i max, F&& fun)
{
    CutResult<float>::rect hole;

    //constexpr auto eps = Vector2{.125f};
    if (c.find_hole_in_bbox(hole, Vector2(min) /*- eps*/, Vector2(max) /*+ eps*/))
    {
        fun(min, max);
    }
    else
    {
        fm_assert(Vector2(Vector2i(hole.min)) == hole.min);
        fm_assert(Vector2(Vector2i(hole.max)) == hole.max);
        auto res = CutResult<Int>::cut(min, max, Vector2i(hole.min), Vector2i(hole.max));
        if (!res.found())
        {
            fun(min, max);
        }
        else
        {
            for (auto i = 0u; i < res.size; i++)
            {
                const auto [min, max] = res.array[i];
                cut_holes_in_wall<IsWest>(c, tile, min, max, fun);
            }
        }
    }
}

struct quad_tuple
{
    Vector2ui tex_pos, tex_size;
    Vector2i min, max;
};

template<bool IsWest>
quad_tuple get_wall_quad_stuff(const CutResult<Int>::rect& geom, const CutResult<Int>::rect& orig,
                               Vector2ui orig_tex_pos, Vector2ui orig_tex_size)
{
    return {};
}

ArrayView<const Quads::indexes> make_indexes(uint32_t count)
{
    static auto arrayʹ = [] {
        auto array = Array<Quads::indexes>{};
        arrayReserve(array, 64);
        return array;
    }();
    auto& array = arrayʹ;
    auto i = (uint32_t)array.size();
    if (count > i) [[unlikely]]
    {
        arrayResize(array, NoInit, count);
        for (; i < count; i++)
            array.data()[i] = Quads::quad_indexes(i);
    }
    return array.prefix(count);
}

Array<std::array<chunk::vertex, 4>>& make_vertexes()
{
    static auto array = [] {
        Array<std::array<chunk::vertex, 4>> array;
        arrayReserve(array, 32);
        return array;
    }();
    return array;
}

template<Group_ G, bool IsWest>
constexpr float depth_offset_for_group()
{
    static_assert(G < Group_::COUNT);
    float p = IsWest ? tile_shader::wall_west_offset : 0;
    switch (G)
    {
    default:
        return tile_shader::wall_depth_offset + p;
    case Group_::corner:
    case Group_::side:
        return tile_shader::wall_side_offset + p;
    }
}

Frame variant_from_frame(ArrayView<const Frame> frames, global_coords coord, variant_t variant, bool is_west)
{
    auto sz = (unsigned)frames.size();
    if (variant == (variant_t)-1)
        variant = (variant_t)(Vector2ui(coord.raw()).sum() + uint32_t{is_west});
    variant %= sz;
    return frames[variant];
}

constexpr std::array<chunk::vertex, 4>& alloc_wall_vertexes(uint32_t& N, auto& V, auto& M, uint32_t k)
{
    constexpr uint32_t reserve = 15, mask = ~reserve;
    const auto i = N, sz = ++N + reserve & mask;
    fm_assert(uint32_t{(UnsignedShort)sz} == sz);
    arrayResize(V, NoInit, sz);
    arrayResize(M, NoInit, sz);
    M[i] = (uint16_t)k;
    return V[i];
};

template<Group_ G, bool IsWest>
void do_wall_part(const Group& group, wall_atlas& A, chunk& c, chunk::wall_stuff& W,
                  Array<std::array<chunk::vertex, 4>>& vertexes,
                  global_coords coord, uint32_t& N, uint32_t tile)
{
    if (!group.is_defined)
        return;

    const uint32_t k = tile*2 + IsWest;
    constexpr auto D = IsWest ? Direction_::W : Direction_::N;
    const auto variant_2 = W.variants[k];
    const auto pos = local_coords{tile};
    const auto center = Vector3(pos) * TILE_SIZE;
    const auto& dir = A.calc_direction(D);
    const auto Depth = A.info().depth;

    if constexpr(G == Group_::side) [[unlikely]]
    {
        bool corner_ok = false, pillar_ok = false;

        if constexpr(!IsWest)
        {
            if (auto t = c.at_offset(pos, {-1, 0}); !(t && t->wall_north_atlas()))
            {
                if (W.atlases[k + 1]) // west on same tile
                    pillar_ok = true;
                if (auto t = c.at_offset(pos, {0, -1}); t && t->wall_west_atlas())
                    corner_ok = true;
            }
        }
        else
        {
            if (auto t = c.at_offset(pos, {0, -1}); !(t && t->wall_west_atlas()))
                if (auto t = c.at_offset(pos, {-1, 0}); t && t->wall_north_atlas())
                    corner_ok = true;
        }

        if (pillar_ok) [[unlikely]]
        {
            if (dir.top.is_defined)
            {
                const auto frames = A.frames(dir.top);
                const auto frame = variant_from_frame(frames, coord, variant_2, IsWest);
                constexpr Vector2 half_tile = TILE_SIZE2 * .5f;
                constexpr float X = half_tile.x(), Y = half_tile.y(), Z = TILE_SIZE.z();
                const auto Depthʹ = (float)(int)Depth;
                Quads::quad quad = {{
                    {-X - Depthʹ, -Y - Depthʹ, Z},
                    {-X,          -Y - Depthʹ, Z},
                    {-X - Depthʹ, -Y,          Z},
                    {-X,          -Y,          Z},
                }};
                fm_assert(frame.size.x() == Depth);
                fm_assert(frame.size.y() >= Depth);
                auto start = frame.offset + Vector2ui{0, frame.size.y()} - Vector2ui{0, Depth};
                const auto texcoords = Quads::texcoords_at(start, Vector2ui{Depth, Depth}, A.image_size());
                const auto depth_offset = depth_offset_for_group<Group_::top, IsWest>();
                const auto depth = tile_shader::depth_value(pos, depth_offset);
                for (auto& v : quad)
                    v += center;
                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j], texcoords[j], depth};
            }
        }
        if (corner_ok) [[unlikely]]
        {
            if (dir.corner.is_defined)
            {
                const auto frames = A.frames(dir.corner);
                const auto depth_offset = depth_offset_for_group<Group_::corner, IsWest>();
                const auto pos_x = !IsWest ? (float)pos.x : (float)pos.x - 1;
                const auto depth = tile_shader::depth_value(pos_x, pos.y, depth_offset);
                const auto& frame = variant_from_frame(frames, coord, variant_2, IsWest);
                const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, A.image_size());
                auto quad = get_quad<Group_::corner, IsWest>((float)Depth);
                for (auto& v : quad)
                    v += center;
                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j], texcoords[j], depth};
            }
            else if (dir.wall.is_defined) [[likely]]
            {
                const auto frames = A.frames(dir.wall);
                const auto depth_offset = depth_offset_for_group<Group_::corner, IsWest>();
                const auto depth = tile_shader::depth_value(!IsWest ? (float)pos.x : (float)pos.x - 1, (float)pos.y, depth_offset);
                const auto frame = variant_from_frame(frames, coord, variant_2, IsWest);
                fm_assert(frame.size.x() > Depth);
                auto start = frame.offset + Vector2ui{frame.size.x(), 0} - Vector2ui{Depth, 0};
                const auto texcoords = Quads::texcoords_at(start, {Depth, frame.size.y()}, A.image_size());
                auto quad = get_quad<Group_::corner, IsWest>((float)Depth);
                for (auto& v : quad)
                    v += center;
                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j], texcoords[j], depth};
            }
        }
    }

    {
        const auto frames = A.frames(group);
        const auto frame = variant_from_frame(frames, coord, variant_2, IsWest);
        const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, A.image_size());
        const auto depth_offset = depth_offset_for_group<G, IsWest>();
        const auto depth = tile_shader::depth_value(pos, depth_offset);
        auto quad = get_quad<G, IsWest>((float)Depth);
        for (auto& v : quad)
            v += center;
        auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
        for (uint8_t j = 0; j < 4; j++)
            v[j] = {quad[j], texcoords[j], depth};
    }
}

} // namespace

GL::Mesh chunk::make_wall_mesh()
{
    fm_debug_assert(_walls);

    uint32_t N = 0;
#ifdef __CLION_IDE__
    extern const uint32_t _foo; N = _foo;
#endif

    auto& vertexes = make_vertexes();
    auto& W = *_walls;

    arrayResize(vertexes, 0);

    for (uint32_t k = 0; k < TILE_COUNT; k++)
    {
        const auto coord = global_coords{_coord, local_coords{k}};

        static_assert(Wall::Group_COUNT == 4);
        static_assert((int)Direction_::COUNT == 2);

        if (auto* A_nʹ = W.atlases[k*2 + 0].get())
        {
            auto& A_n = *A_nʹ;
            const auto& dir = A_n.calc_direction(Direction_::N);
            do_wall_part<Group_::wall, false>(dir.wall, A_n, *this, W, vertexes, coord, N, k);
            do_wall_part<Group_::side, false>(dir.side, A_n, *this, W, vertexes, coord, N, k);
            do_wall_part<Group_::top,  false>(dir.top,  A_n, *this, W, vertexes, coord, N, k);
        }
        if (auto* A_wʹ = W.atlases[k*2 + 1].get())
        {
            auto& A_w = *A_wʹ;
            const auto& dir = A_w.calc_direction(Direction_::W);
            do_wall_part<Group_::wall,  true>(dir.wall, A_w, *this, W, vertexes, coord, N, k);
            do_wall_part<Group_::side,  true>(dir.side, A_w, *this, W, vertexes, coord, N, k);
            do_wall_part<Group_::top,   true>(dir.top,  A_w, *this, W, vertexes, coord, N, k);
        }
    }

    if (N == 0)
        return GL::Mesh{NoCreate};

    ranges::sort(ranges::zip_view(vertexes.prefix(N),
                                  ArrayView{_walls->mesh_indexes.data(), N}),
                 [&A = _walls->atlases](const auto& a, const auto& b) {
                     const auto& [av, ai] = a;
                     const auto& [bv, bi] = b;
                     return A[ai].get() < A[bi].get();
                 });

    auto vertex_view = std::as_const(vertexes).prefix(N);
    auto index_view = make_indexes(N);

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0,
                         tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{index_view}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(int32_t(6 * N));
    fm_debug_assert((uint32_t)mesh.count() == N*6);
    return mesh;
}

auto chunk::ensure_wall_mesh() noexcept -> wall_mesh_tuple
{
    if (!_walls)
        return {wall_mesh, {}, 0};

    if (!_walls_modified)
        return { wall_mesh, _walls->mesh_indexes, (size_t)wall_mesh.count()/6u };

    _walls_modified = false;
    wall_mesh = make_wall_mesh();
    return { wall_mesh, _walls->mesh_indexes, (size_t)wall_mesh.count()/6u };
}

} // namespace floormat
