#include "chunk.hpp"
#include "tile-bbox.hpp"
#include "quads.hpp"
#include "wall-atlas.hpp"
#include "hole.hpp"
#include "compat/function2.hpp"
#include "RTree-search.hpp"
#include "shaders/shader.hpp"
#include "depth.hpp"
#include "renderer.hpp"
#include <cr/ArrayViewStl.h>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <cr/StructuredBindings.h>
#include <mg/Range.h>
#include <utility>
#include <concepts>
#include <tuple>
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

struct HoleData
{
    Vector2 min, max;
    uint8_t zmin = 0, zmax = 0;
};

ArrayView<HoleData> find_wall_holes_in_world_coords(Array<HoleData>& output, chunk& c, local_coords tile_pos, bool IsWest)
{
    auto wall_bb = [&] -> Optional<Pair<Vector2, Vector2>> {
        auto tile = c[tile_pos];
        if (!IsWest)
        {
            if (const auto* atlas = tile.wall_north_atlas().get())
                return wall_north(tile_pos.to_index(), (float)atlas->info().depth);
        }
        else
        {
            if (const auto* atlas = tile.wall_west_atlas().get())
                return wall_west(tile_pos.to_index(), (float)atlas->info().depth);
        }
        return NullOpt;
    }();

    arrayResize(output, 0);
    arrayReserve(output, 16);

    if (wall_bb)
    {
        c.get_all_holes_in_bbox([&](Math::Range2D<float> bb, Math::Range1D<uint8_t> z) {
            arrayAppend(output, HoleData{ .min = bb.min(), .max = bb.max(), .zmin = z.min(), .zmax = z.max()});
        }, c, wall_bb->first(), wall_bb->second());
    }

    return output;
}

struct WallFragment
{
    Vector2 remove_from_start_xz, remove_from_end_xz;
    Range2D face_coords;
    Range3D world_coords;
};

Array<CutResult<float>::rect> wall_fragments, next_wall_fragments;

ArrayView<WallFragment>
cut_wall_face(Array<WallFragment>& output, ArrayView<HoleData> holes, local_coords tile_pos, bool IsWest)
{
    arrayResize(wall_fragments, 0);
    arrayReserve(wall_fragments, 16);
    arrayResize(next_wall_fragments, 0);
    arrayReserve(next_wall_fragments, 16);
    arrayResize(output, 0);
    arrayReserve(output, 16);

    constexpr auto half_tile = tile_size_xy*.5f;
    const unsigned XAxis = !IsWest ? 0 : 1;

    auto offset = TILE_SIZE2 * Vector2(tile_pos);
    auto off_x = offset[XAxis];
    auto bb_min = Vector2{-half_tile + off_x, 0},
         bb_max = Vector2{half_tile + off_x, tile_size_z};
    arrayAppend(wall_fragments, {bb_min, bb_max});

    for (auto hole : holes)
    {
        arrayResize(next_wall_fragments, 0);

        const auto hole_min = Vector2{hole.min[XAxis], (float)hole.zmin},
                   hole_max = Vector2{hole.max[XAxis], (float)hole.zmax};

        for (auto x : wall_fragments)
        {
            const auto frags = CutResult<float>::cut(x.min, x.max, hole_min, hole_max);
            for (auto i = 0u; i < frags.size; i++)
                arrayAppend(next_wall_fragments, frags.array[i]);
#if 0
            if (frags.s != (uint8_t)-1)
                for (auto i = 0u; i < frags.size; i++)
                    DBG << "good frag" << i << frags.array[i].min << frags.array[i].max;
#endif
        }

        arrayResize(wall_fragments, 0);
        swap(wall_fragments, next_wall_fragments);
    }

#if 0
    DBG << "frags" << wall_fragments.size();
#endif

    arrayResize(output, 0);
    arrayReserve(output, wall_fragments.size());

    for (auto w : wall_fragments)
    {
        Vector3 w_min{NoInit}, w_max{NoInit};
        w_min[XAxis] = w.min.x();
        w_max[XAxis] = w.max.x();
        w_min[1-XAxis] = -half_tile;
        w_max[1-XAxis] = -half_tile;
        w_min[2] = w.min.y();
        w_max[2] = w.max.y();

        auto frag = WallFragment {
            .remove_from_start_xz = w.min - bb_min,
            .remove_from_end_xz = bb_max - w.max,
            .face_coords = { w.min, w.max },
            .world_coords = { w_min, w_max },
        };
        arrayAppend(output, frag);
    }

    arrayResize(wall_fragments, 0);
    arrayResize(next_wall_fragments, 0);

    return output;
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
constexpr int32_t depth_offset_for_group(uint32_t depth)
{
    (void)depth;
    static_assert(G < Group_::COUNT);
    constexpr auto half_tile = tile_size_xy/2;
    int32_t part_offset = 0;
    switch (G)
    {
    case Group_::top:
    case Group_::side:
        part_offset = -1;
        break;
    default:
        break;
    }
    auto ret = -half_tile + part_offset;
    return ret;
}

Frame variant_from_frame(ArrayView<const Frame> frames, global_coords coord, variant_t variant, bool is_west)
{
    auto sz = (unsigned)frames.size();
    if (variant == (variant_t)-1)
        variant = (variant_t)(Vector2ui(coord.raw()).sum() + uint32_t{is_west});
    variant %= sz;
    return frames[variant];
}

Array<HoleData> hole_data;

std::array<chunk::vertex, 4>& alloc_wall_vertexes(uint32_t& N, auto& V, auto& M, uint32_t k)
{
    constexpr uint32_t reserve = 15, mask = ~reserve;
    const auto i = N, sz = ++N + reserve & mask;
    fm_assert(uint32_t{(UnsignedShort)sz} == sz);
    arrayResize(V, NoInit, sz);
    arrayResize(M, NoInit, sz);
    M[i] = (uint16_t)k;
    return V[i];
};

Array<WallFragment> fragdata;

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
    const float depth_start = Render::get_status().is_clipcontrol_clipdepth_zero_one_enabled ? 0.f : -1.f;
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
                const auto depth_offset = depth_offset_for_group<Group_::top, IsWest>(A.depth());
                const auto depth = Depth::value_at(depth_start, {c.coord(), pos, {}}, depth_offset);
                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth};
            }
        }
        if (corner_ok) [[unlikely]]
        {
            if (dir.corner.is_defined)
            {
                const auto frames = A.frames(dir.corner);
                const auto depth_offset = depth_offset_for_group<Group_::corner, IsWest>(A.depth());
                const auto depth = Depth::value_at(depth_start, {c.coord(), pos, {}}, depth_offset);
                const auto& frame = variant_from_frame(frames, coord, variant_2, IsWest);
                const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, A.image_size());

                constexpr Vector2 half_tile = TILE_SIZE2 * .5f;
                constexpr float X = half_tile.x(), Y = half_tile.y(), Z = TILE_SIZE.z();
                const auto Depthʹ = (float)(int)Depth;
                Quads::quad quad = {};
                if constexpr (!IsWest)
                    quad = {{
                        {-X,          -Y, 0},
                        {-X,          -Y, Z},
                        {-X - Depthʹ, -Y, 0},
                        {-X - Depthʹ, -Y, Z},
                    }};
                else
                    quad = {{
                        {-X, -Y - Depthʹ, 0},
                        {-X, -Y - Depthʹ, Z},
                        {-X, -Y,          0},
                        {-X, -Y,          Z},
                    }};

                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth};
            }
            else if (dir.wall.is_defined) [[likely]]
            {
                const auto frames = A.frames(dir.wall);
                const auto depth_offset = depth_offset_for_group<Group_::corner, IsWest>(A.depth());
                const auto depth = Depth::value_at(depth_start, {c.coord(), pos, {}}, depth_offset);
                const auto frame = variant_from_frame(frames, coord, variant_2, IsWest);
                fm_assert(frame.size.x() > Depth);
                auto start = frame.offset + Vector2ui{frame.size.x(), 0} - Vector2ui{Depth, 0};
                const auto texcoords = Quads::texcoords_at(start, {Depth, frame.size.y()}, A.image_size());

                constexpr Vector2 half_tile = TILE_SIZE2 * .5f;
                constexpr float X = half_tile.x(), Y = half_tile.y(), Z = TILE_SIZE.z();
                const auto Depthʹ = (float)(int)Depth;
                Quads::quad quad = {};
                if constexpr (!IsWest)
                    quad = {{
                        {-X,          -Y, 0},
                        {-X,          -Y, Z},
                        {-X - Depthʹ, -Y, 0},
                        {-X - Depthʹ, -Y, Z},
                    }};
                else
                    quad = {{
                        {-X, -Y - Depthʹ, 0},
                        {-X, -Y - Depthʹ, Z},
                        {-X, -Y,          0},
                        {-X, -Y,          Z},
                    }};

                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth};
            }
        }
    }

    {
        auto holes = find_wall_holes_in_world_coords(hole_data, c, pos, IsWest);
        cut_wall_face(fragdata, holes, pos, IsWest);

        const auto frames = A.frames(group);
        const auto frame = variant_from_frame(frames, coord, variant_2, IsWest);
        const auto depth_offset = depth_offset_for_group<G, IsWest>(A.depth());
        const auto depth = Depth::value_at(depth_start, {c.coord(), pos, {}}, depth_offset);

        constexpr Vector2 half_tile = TILE_SIZE2 * .5f;
        constexpr float X = half_tile.x(), Y = half_tile.y(), Z = TILE_SIZE.z();
        const auto Depthʹ = (float)(int)Depth;
        const auto image_size = A.image_size();

        for (const auto& frag : fragdata)
        {
            const auto& rs = frag.remove_from_start_xz;
            const auto& re = frag.remove_from_end_xz;

            // face-start maps to texture-left for north, texture-right for west.
            // tex_left/tex_right are the pixel-x trims from left/right of the texture.
            const auto tex_left  = !IsWest ? (unsigned)rs.x() : (unsigned)re.x();
            const auto tex_right = !IsWest ? (unsigned)re.x() : (unsigned)rs.x();

            if constexpr (G == Group_::wall)
            {
                // wall texture: {tile_size_xy, tile_size_z}
                // pixel x = primary axis, pixel y = z (low pixel y = high z)
                auto sub_offset = frame.offset + Vector2ui(tex_left, (unsigned)re.y());
                auto sub_size   = frame.size - Vector2ui(tex_left + tex_right, (unsigned)rs.y() + (unsigned)re.y());
                if (!sub_size.x() || !sub_size.y())
                    continue;
                const auto texcoords = Quads::texcoords_at(sub_offset, sub_size, image_size);

                Quads::quad quad = {};
                if constexpr (!IsWest)
                    quad = {{
                        { X - re.x(), -Y, rs.y()},
                        { X - re.x(), -Y, Z - re.y()},
                        {-X + rs.x(), -Y, rs.y()},
                        {-X + rs.x(), -Y, Z - re.y()},
                    }};
                else
                    quad = {{
                        {-X, -Y + rs.x(), rs.y()},
                        {-X, -Y + rs.x(), Z - re.y()},
                        {-X,  Y - re.x(), rs.y()},
                        {-X,  Y - re.x(), Z - re.y()},
                    }};

                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth};
            }
            else if constexpr (G == Group_::side)
            {
                auto sub_offset = frame.offset + Vector2ui(0, (unsigned)re.y());
                auto sub_size   = Vector2ui(frame.size.x(), frame.size.y() - (unsigned)rs.y() - (unsigned)re.y());
                if (!sub_size.y())
                    continue;
                const auto texcoords = Quads::texcoords_at(sub_offset, sub_size, image_size);

                const auto frag_x1 =  X - re.x();
                const auto frag_y1 =  Y - re.x();

                Quads::quad quad = {};
                if constexpr (!IsWest)
                    quad = {{
                        {frag_x1, -Y - Depthʹ, rs.y()},
                        {frag_x1, -Y - Depthʹ, Z - re.y()},
                        {frag_x1, -Y,          rs.y()},
                        {frag_x1, -Y,          Z - re.y()},
                    }};
                else
                    quad = {{
                        {-X,          frag_y1, rs.y()},
                        {-X,          frag_y1, Z - re.y()},
                        {-X - Depthʹ, frag_y1, rs.y()},
                        {-X - Depthʹ, frag_y1, Z - re.y()},
                    }};

                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth};
            }
            else if constexpr (G == Group_::top)
            {
                auto sub_offset = frame.offset + Vector2ui(0, tex_right);
                auto sub_size   = Vector2ui(frame.size.x(), frame.size.y() - tex_left - tex_right);
                if (!sub_size.y())
                    continue;
                const auto texcoords = Quads::texcoords_at(sub_offset, sub_size, image_size);

                const auto frag_z = Z - re.y();
                Quads::quad quad = {};
                if constexpr (!IsWest)
                    quad = {{
                        {-X + rs.x(), -Y - Depthʹ, frag_z},
                        { X - re.x(), -Y - Depthʹ, frag_z},
                        {-X + rs.x(), -Y,          frag_z},
                        { X - re.x(), -Y,          frag_z},
                    }};
                else
                    quad = {{
                        {-X,          -Y + rs.x(), frag_z},
                        {-X,           Y - re.x(), frag_z},
                        {-X - Depthʹ, -Y + rs.x(), frag_z},
                        {-X - Depthʹ,  Y - re.x(), frag_z},
                    }};

                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth};
            }
            else
            {
                static_assert(G == Group_::corner);
                if (&frag != fragdata.data())
                    continue;

                const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, image_size);
                Quads::quad quad = {};
                if constexpr (!IsWest)
                    quad = {{
                        {-X,          -Y, 0},
                        {-X,          -Y, Z},
                        {-X - Depthʹ, -Y, 0},
                        {-X - Depthʹ, -Y, Z},
                    }};
                else
                    quad = {{
                        {-X, -Y - Depthʹ, 0},
                        {-X, -Y - Depthʹ, Z},
                        {-X, -Y,          0},
                        {-X, -Y,          Z},
                    }};

                auto& v = alloc_wall_vertexes(N, vertexes, W.mesh_indexes, k);
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth};
            }
        }
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

        static_assert(Wall::Group_COUNT == /* 5 */ 4);
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
