#include "chunk.hpp"
#include "tile-bbox.hpp"
#include "quads.hpp"
#include "wall-atlas.hpp"
#include "shaders/shader.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/PairStl.h>
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

// -----------------------

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
                { X, -Y, Z },
                { X, -Y, 0 },
                {-X, -Y, Z },
                {-X, -Y, 0 },
            }};
        else
            return {{
                {-X, -Y, Z },
                {-X, -Y, 0 },
                {-X,  Y, Z },
                {-X,  Y, 0 },
            }};
    case corner_L: {
            constexpr float x_offset = (float)(unsigned)X;
            return {{
                { -X + x_offset, -Y, Z },
                { -X + x_offset, -Y, 0 },
                { -X, -Y, Z },
                { -X, -Y, 0 },
            }};
        }
        case corner_R: {
            constexpr float y_offset = TILE_SIZE.y() - (float)(unsigned)Y;
            return {{
                {-X,  -Y, Z },
                {-X,  -Y, 0 },
                {-X,  -Y + y_offset, Z },
                {-X,  -Y + y_offset, 0 },
            }};
        }
    case side:
        if (!is_west)
        {
            return {{
                { X, -Y - depth, Z },
                { X, -Y - depth, 0 },
                { X, -Y, Z },
                { X, -Y, 0 },
            }};
        }
        else
        {
            return {{
                { -X, Y, Z },
                { -X, Y, 0 },
                { -X - depth, Y, Z },
                { -X - depth, Y, 0 },
            }};
      }
    case top:
        if (!is_west)
        {
            return {{
                { -X, -Y - depth, Z },
                {  X, -Y - depth, Z },
                { -X, -Y, Z },
                {  X, -Y, Z }
            }};
        }
        else
        {
            return {{
                { -X - depth, -Y, Z },
                { -X, -Y, Z },
                { -X - depth, Y, Z },
                { -X, Y, Z }
            }};
        }
    }
    std::unreachable();
    fm_abort("invalid wall_atlas group '%d'", (int)G);
}

// -----------------------

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
    fm_assert(max < chunk::max_wall_quad_count);
    return indexes.prefix(max);
}

constexpr auto depth_offset_for_group(Group_ G)
{
    CORRADE_ASSUME(G < Group_::COUNT);
    switch (G)
    {
    default:
        return tile_shader::wall_depth_offset;
    case Wall::Group_::corner_L:
    case Wall::Group_::corner_R:
        return tile_shader::wall_overlay_depth_offset;
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
        const auto D = k & 1 ? Wall::Direction_::W : Wall::Direction_::N;
        const auto& atlas = _walls->atlases[k];
        if (!atlas)
            continue;
        const auto variant_ = _walls->variants[k];
        const auto pos = local_coords{k / 2u};
        const auto center = Vector3(pos) * TILE_SIZE;
        const auto& dir = atlas->calc_direction(D);
        const auto vpos = (uint8_t)Vector2ui(global_coords{_coord, pos}.raw()).sum();

        for (auto [_, member, G] : Wall::Direction::groups)
        {
            CORRADE_ASSUME(G < Group_::COUNT);

            switch (G)
            {
            case Wall::Group_::corner_L:
                if (D != Direction_::N || !_walls->atlases[k+1])
                    continue;
                break;
            case Wall::Group_::corner_R:
                if (D != Direction_::W || !_walls->atlases[k-1])
                    continue;
                break;
            default:
                break;
            }

            const auto& group = dir.*member;
            if (!group.is_defined)
                continue;
            const auto depth_offset = depth_offset_for_group(G);
            auto quad = get_quad(D, G, atlas->info().depth);
            for (auto& v : quad)
                v += center;

            const auto i = N++;
            fm_debug_assert(i < max_wall_quad_count);
            _walls->mesh_indexes[i] = (uint16_t)k;
            const auto frames = atlas->frames(group);
            const auto variant = (variant_ != (uint8_t)-1 ? variant_ : vpos) % frames.size();
            const auto& frame = frames[variant];
            const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, atlas->image_size());
            const auto depth = tile_shader::depth_value(pos, depth_offset);
            fm_debug_assert(i < vertexes.size());
            auto& v = vertexes[i];
            for (uint8_t j = 0; j < 4; j++)
                v[j] = { quad[j], texcoords[j], depth };
        }
    }

    ranges::sort(ranges::zip_view(vertexes.prefix(N),
                                  ArrayView<uint_fast16_t>{_walls->mesh_indexes.data(), N}),
                 [&A = _walls->atlases](const auto& a, const auto& b) { return A[a.second] < A[b.second]; });

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
