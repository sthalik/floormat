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

using Wall::Group_;
using Wall::Direction_;

constexpr Quads::quad get_quad(Direction_ D, Group_ G, float depth)
{
    CORRADE_ASSUME(D < Direction_::COUNT);
    CORRADE_ASSUME(G < Group_::COUNT);
    constexpr Vector2 half_tile = TILE_SIZE2*.5f;
    constexpr float X = half_tile.x(), Y = half_tile.y(), Z = TILE_SIZE.z();
    const bool is_west = D == Wall::Direction_::W;

    switch (G)
    {
    using enum Group_;
    case COUNT:
        std::unreachable();
    case wall:
        if (!is_west)
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
        if (!is_west)
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
        if (!is_west)
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
        if (!is_west)
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
    fm_abort("invalid wall_atlas group '%d'", (int)G);
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

constexpr float depth_offset_for_group(Group_ G, bool is_west)
{
    CORRADE_ASSUME(G < Group_::COUNT);
    float p = is_west ? tile_shader::wall_west_offset : 0;
    switch (G)
    {
    default:
        return tile_shader::wall_depth_offset + p;
    case Wall::Group_::corner:
    case Wall::Group_::side:
        return tile_shader::wall_side_offset + p;
    }
}

} // namespace

GL::Mesh chunk::make_wall_mesh()
{
    fm_debug_assert(_walls);
    uint32_t N = 0;

    static auto vertexes = Array<std::array<vertex, 4>>{NoInit, max_wall_quad_count };

    for (uint32_t k = 0; k < 2*TILE_COUNT; k++)
    {
        const bool is_west = k & 1;
        const auto D = is_west ? Wall::Direction_::W : Wall::Direction_::N;
        const auto& atlas = _walls->atlases[k];
        if (!atlas)
            continue;
        const auto variant_ = _walls->variants[k];
        const auto pos = local_coords{k / 2u};
        const auto center = Vector3(pos) * TILE_SIZE;
        const auto& dir = atlas->calc_direction(D);
        const auto vpos = (uint8_t)(Vector2ui(global_coords{_coord, pos}.raw()).sum() + is_west);
        const auto Depth = atlas->info().depth;

        for (auto [_, member, G] : Wall::Direction::groups_for_draw)
        {
            CORRADE_ASSUME(G < Group_::COUNT);

            bool side_ok = G == Wall::Group_::side;

            if (!(dir.*member).is_defined)
                continue;

            if (G == Wall::Group_::side) [[unlikely]]
            {
                bool corner_ok = false, pillar_ok = false;

                if (!is_west)
                {
                    if (auto t = at_offset_(pos, {-1, 0}); !(t && t->wall_north_atlas()))
                    {
                        if (_walls->atlases[k+1]) // west on same tile
                            pillar_ok = true;
                        if (auto t = at_offset_(pos, {0, -1}); t && t->wall_west_atlas())
                            corner_ok = true;
                    }
                    if (side_ok)
                        if (auto t = at_offset_(pos, {1, -1}); t && t->wall_west_atlas())
                            side_ok = false;
                    if (side_ok)
                        if (auto t = at_offset_(pos, {1, -1}); t && t->wall_west_atlas())
                            side_ok = false;
                }
                else
                {
                    if (auto t = at_offset_(pos, {0, -1}); !(t && t->wall_west_atlas()))
                        if (auto t = at_offset_(pos, {-1, 0}); t && t->wall_north_atlas())
                            corner_ok = true;
                    if (side_ok)
                        if (auto t = at_offset_(pos, {-1, 1}); t && t->wall_north_atlas())
                            side_ok = false;
                }

                if (pillar_ok) [[unlikely]]
                {
                    if (dir.top.is_defined)
                    {
                        const auto frames = atlas->frames(dir.top);
                        auto variant = (variant_ != (uint8_t)-1 ? variant_ : vpos);
                        variant += (uint8_t)(!is_west ? frames.size() - 1 : 1);
                        fm_assert((size_t)(variant_t)frames.size() == frames.size());
                        variant = variant % (variant_t)frames.size();
                        constexpr Vector2 half_tile = TILE_SIZE2*.5f;
                        constexpr float X = half_tile.x(), Y = half_tile.y(), Z = TILE_SIZE.z();
                        Quads::quad quad = {{
                            { -X - Depth, -Y - Depth, Z },
                            { -X,         -Y - Depth, Z },
                            { -X - Depth, -Y, Z },
                            { -X,         -Y, Z }
                        }};
                        const auto& frame = frames[variant];
                        fm_assert(frame.size.x() == Depth);
                        fm_assert(frame.size.y() >= Depth);
                        auto start = frame.offset + Vector2ui{0, frame.size.y()} - Vector2ui{0, Depth};
                        const auto texcoords = Quads::texcoords_at(start, Vector2ui{Depth, Depth}, atlas->image_size());
                        const auto i = N++;
                        fm_assert(i < vertexes.size());
                        _walls->mesh_indexes[i] = (uint16_t)k;
                        const auto depth_offset = depth_offset_for_group(Group_::top, is_west);
                        const auto depth = tile_shader::depth_value(pos, depth_offset);
                        auto& v = vertexes[i];
                        for (auto& v : quad)
                            v += center;
                        for (uint8_t j = 0; j < 4; j++)
                            v[j] = { quad[j], texcoords[j], depth };
                    }
                }
                if (corner_ok) [[unlikely]]
                {
                    if (dir.corner.is_defined)
                    {
                        const auto frames = atlas->frames(dir.corner);
                        auto variant = (variant_ != (uint8_t)-1 ? variant_ : vpos);
                        const auto depth_offset = depth_offset_for_group(Group_::corner, is_west);
                        const auto pos_x = !is_west ? (float)pos.x : (float)pos.x - 1;
                        const auto depth = tile_shader::depth_value(pos_x, pos.y, depth_offset);
                        variant += variant_t(!is_west ? frames.size() - 1 : 1);
                        fm_assert((size_t)(variant_t)frames.size() == frames.size());
                        variant = variant % (variant_t)frames.size();
                        const auto& frame = frames[variant];
                        const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, atlas->image_size());
                        const auto i = N++;
                        fm_assert(i < vertexes.size());
                        _walls->mesh_indexes[i] = (uint16_t)k;
                        auto& v = vertexes[i];
                        auto quad = get_quad(D, Group_::corner, (float)Depth);
                        for (auto& v : quad)
                            v += center;
                        for (uint8_t j = 0; j < 4; j++)
                            v[j] = { quad[j], texcoords[j], depth };
                    }
                    else if (dir.wall.is_defined) [[likely]]
                    {
                        const auto frames = atlas->frames(dir.wall);
                        auto variant = (variant_ != (uint8_t)-1 ? variant_ : vpos);
                        const auto depth_offset = depth_offset_for_group(Group_::corner, is_west);
                        const auto depth = tile_shader::depth_value(!is_west ? (float)pos.x : (float)pos.x - 1, depth_offset);
                        variant += variant_t(!is_west ? frames.size() - 1 : 1);
                        variant %= frames.size();
                        const auto& frame = frames[variant];
                        fm_assert(frame.size.x() > Depth);
                        auto start = frame.offset + Vector2ui{frame.size.x(), 0} - Vector2ui{Depth, 0};
                        const auto texcoords = Quads::texcoords_at(start, {Depth, frame.size.y()}, atlas->image_size());
                        const auto i = N++;
                        fm_assert(i < vertexes.size());
                        _walls->mesh_indexes[i] = (uint16_t)k;
                        auto& v = vertexes[i];
                        auto quad = get_quad(D, Group_::corner, (float)Depth);
                        for (auto& v : quad)
                            v += center;
                        for (uint8_t j = 0; j < 4; j++)
                            v[j] = { quad[j], texcoords[j], depth };
                    }
                }
            }

            if (G != Wall::Group_::side || side_ok) [[likely]]
            {
                const auto& group = dir.*member;
                const auto frames = atlas->frames(group);

                const auto i = N++;
                fm_assert(i < vertexes.size());
                _walls->mesh_indexes[i] = (uint16_t)k;

                const auto variant = (variant_ != (uint8_t)-1 ? variant_ : vpos) % frames.size();
                const auto& frame = frames[variant];
                const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, atlas->image_size());
                const auto depth_offset = depth_offset_for_group(G, is_west);
                const auto depth = tile_shader::depth_value(pos, depth_offset);
                auto quad = get_quad(D, G, (float)Depth);
                for (auto& v : quad)
                    v += center;
                auto& v = vertexes[i];
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = { quad[j], texcoords[j], depth };
            }
        }
    }

    ranges::sort(ranges::zip_view(vertexes.prefix(N),
                                  ArrayView<uint_fast16_t>{_walls->mesh_indexes.data(), N}),
                 [&A = _walls->atlases](const auto& a, const auto& b) {
                     const auto& [av, ai] = a;
                     const auto& [bv, bi] = b;
                     return A[ai] < A[bi];
                 });

    auto vertex_view = std::as_const(vertexes).prefix(N);
    auto index_view = make_indexes(N);

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0,
                         tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{index_view}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(int32_t(6 * N));
    fm_debug_assert((size_t)mesh.count() == N*6);
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
