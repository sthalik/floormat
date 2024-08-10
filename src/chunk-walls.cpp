#include "chunk.hpp"
#include "tile-bbox.hpp"
#include "quads.hpp"
#include "wall-atlas.hpp"
#include "shaders/shader.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/Optional.h>
#include <utility>
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

template<Group_ G, bool IsWest>
constexpr Quads::quad get_quad(float depth)
{
    CORRADE_ASSUME(G < Group_::COUNT);
    constexpr Vector2 half_tile = TILE_SIZE2*.5f;
    constexpr float X = half_tile.x(), Y = half_tile.y(), Z = TILE_SIZE.z();

    fm_assert(G < Group_::COUNT);
    switch (G)
    {
    using enum Group_;
    case COUNT: std::unreachable();
    case wall:
        if (!IsWest)
            return {{
                { X, -Y, 0 },
                { X, -Y, Z },
                {-X, -Y, 0 },
                {-X, -Y, Z },
            }};
        else
            return {{
                {-X, -Y, 0 },
                {-X, -Y, Z },
                {-X,  Y, 0 },
                {-X,  Y, Z },
            }};
    case side:
        if (!IsWest)
            return {{
                { X, -Y - depth, 0 },
                { X, -Y - depth, Z },
                { X, -Y, 0 },
                { X, -Y, Z },
            }};
        else
            return {{
                { -X, Y, 0 },
                { -X, Y, Z },
                { -X - depth, Y, 0 },
                { -X - depth, Y, Z },
            }};
    case top:
        if (!IsWest)
            return {{
                { -X, -Y - depth, Z },
                {  X, -Y - depth, Z },
                { -X, -Y, Z },
                {  X, -Y, Z },
            }};
        else
            return {{
                { -X, -Y, Z },
                { -X,  Y, Z },
                { -X - depth, -Y, Z },
                { -X - depth,  Y, Z },
            }};
    case corner:
        if (!IsWest)
            return {{
                {-X, -Y, 0 },
                {-X, -Y, Z },
                {-X - depth, -Y, 0 },
                {-X - depth, -Y, Z },
            }};
        else
            return {{
                {-X, -Y - depth, 0 },
                {-X, -Y - depth, Z },
                {-X, -Y, 0 },
                {-X, -Y, Z },
            }};
    }
    std::unreachable();
}

Array<Quads::indexes> make_indexes_()
{
    auto array = Array<Quads::indexes>{NoInit, chunk::max_wall_quad_count };
    for (auto i = 0uz; i < chunk::max_wall_quad_count; i++)
        array[i] = Quads::quad_indexes(i);
    return array;
}

ArrayView<const Quads::indexes> make_indexes(size_t max)
{
    static const auto indexes = make_indexes_();
    fm_assert(max <= chunk::max_wall_quad_count);
    return indexes.prefix(max);
}

template<Group_ G, bool IsWest>
constexpr float depth_offset_for_group()
{
    CORRADE_ASSUME(G < Group_::COUNT);
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
    bool side_ok = true;

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
            if (side_ok)
                if (auto t = c.at_offset(pos, {1, -1}); t && t->wall_west_atlas())
                    side_ok = false;
            if (side_ok)
                if (auto t = c.at_offset(pos, {1, -1}); t && t->wall_west_atlas())
                    side_ok = false;
        }
        else
        {
            if (auto t = c.at_offset(pos, {0, -1}); !(t && t->wall_west_atlas()))
                if (auto t = c.at_offset(pos, {-1, 0}); t && t->wall_north_atlas())
                    corner_ok = true;
            if (side_ok)
                if (auto t = c.at_offset(pos, {-1, 1}); t && t->wall_north_atlas())
                    side_ok = false;
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
                const auto i = N++;
                fm_assert(i < vertexes.size());
                W.mesh_indexes[i] = (uint16_t)k;
                const auto depth_offset = depth_offset_for_group<Group_::top, IsWest>();
                const auto depth = tile_shader::depth_value(pos, depth_offset);
                for (auto& v : quad)
                    v += center;
                auto& v = vertexes[i];
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
                const auto i = N++;
                fm_assert(i < vertexes.size());
                W.mesh_indexes[i] = (uint16_t)k;
                auto quad = get_quad<Group_::corner, IsWest>((float)Depth);
                for (auto& v : quad)
                    v += center;
                auto& v = vertexes[i];
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
                const auto i = N++;
                fm_assert(i < vertexes.size());
                W.mesh_indexes[i] = (uint16_t)k;
                auto quad = get_quad<Group_::corner, IsWest>((float)Depth);
                for (auto& v : quad)
                    v += center;
                auto& v = vertexes[i];
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j], texcoords[j], depth};
            }
        }
    }

    if constexpr(G == Group_::side)
        void();
    else if (!side_ok) [[unlikely]]
        return;

    {
        const auto frames = A.frames(group);
        const auto i = N++;
        fm_assert(i < vertexes.size());
        W.mesh_indexes[i] = (uint16_t)k;
        const auto frame = variant_from_frame(frames, coord, variant_2, IsWest);
        const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, A.image_size());
        const auto depth_offset = depth_offset_for_group<G, IsWest>();
        const auto depth = tile_shader::depth_value(pos, depth_offset);
        auto quad = get_quad<G, IsWest>((float)Depth);
        for (auto& v : quad)
            v += center;
        auto& v = vertexes[i];
        for (uint8_t j = 0; j < 4; j++)
            v[j] = {quad[j], texcoords[j], depth};
    }
}

} // namespace

GL::Mesh chunk::make_wall_mesh()
{
    fm_debug_assert(_walls);
    uint32_t N = 0;

    static auto vertexes = Array<std::array<vertex, 4>>{NoInit, max_wall_quad_count };
    auto& W = *_walls;

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
