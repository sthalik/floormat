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
            auto left  = Vector2{X,        -Y               },
                 right = Vector2{left.x(), left.y() - depth };
            return {{
                        { right.x(), right.y(), Z },
                        { right.x(), right.y(), 0 },
                        { left.x(),  left.y(),  Z },
                        { left.x(),  left.y(),  0 },
                    }};
        }
        else
        {
            auto right = Vector2{ -X,                Y         };
            auto left  = Vector2{ right.x() - depth, right.y() };
            return {{
                        { right.x(), right.y(), Z },
                        { right.x(), right.y(), 0 },
                        { left.x(),  left.y(),  Z },
                        { left.x(),  left.y(),  0 },
          }};
      }
    case top:
        if (!is_west)
        {
            auto top_right    = Vector2{X,              Y - depth        },
                 bottom_right = Vector2{top_right.x(),  Y                },
                 top_left     = Vector2{-X,             top_right.y()    },
                 bottom_left  = Vector2{top_left.x(),   bottom_right.y() };
            return {{
                { top_right.x(),    top_right.y(),    Z }, // br tr
                { top_left.x(),     top_left.y(),     Z }, // tr tl
                { bottom_right.x(), bottom_right.y(), Z }, // bl br
                { bottom_left.x(),  bottom_left.y(),  Z }, // tl bl
            }};
        }
        else
        {
            auto top_right    = Vector2{-X,                    -Y               },
                 top_left     = Vector2{top_right.x() - depth, top_right.y()    },
                 bottom_right = Vector2{top_right.x(),         Y                },
                 bottom_left  = Vector2{top_left.x(),          bottom_right.y() };
            return {{
                { bottom_right.x(), bottom_right.y(), Z },
                { top_right.x(),    top_right.y(),    Z },
                { bottom_left.x(),  bottom_left.y(),  Z },
                { top_left.x(),     top_left.y(),     Z },
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
        fm_assert(atlas != nullptr);
        const auto variant = _walls->variants[k];
        const auto pos = local_coords{k / 2u};
        const auto center = Vector3(pos) * TILE_SIZE;
        const auto& dir = atlas->calc_direction(D);

        for (auto [_, member, G] : Wall::Direction::groups)
        {
            CORRADE_ASSUME(G < Group_::COUNT);

            if (G == Group_::corner_L && D != Direction_::N ||
                G == Group_::corner_R && D != Direction_::W) [[unlikely]]
                continue;

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
            const auto& frame = frames[variant];
            const auto texcoords = Quads::texcoords_at(frame.offset, frame.size, atlas->image_size());
            const auto depth = tile_shader::depth_value(pos, depth_offset);
            auto& v = vertexes[i];
            for (uint8_t j = 0; j < 4; j++)
                v[j] = { quad[j], texcoords[j], depth };
        }
    }

    const auto comp = [&atlases = _walls->atlases](const auto& a, const auto& b) {
        return atlases[a.second] < atlases[b.second];
    };

    ranges::sort(ranges::zip_view(vertexes, _walls->mesh_indexes), comp);

    auto vertex_view = std::as_const(vertexes).prefix(N);
    auto index_view = make_indexes(N);

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
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
        return { wall_mesh, _walls->mesh_indexes, size_t(wall_mesh.count()/6) };
    _walls_modified = false;

#if 0
    size_t count = 0;
    for (auto i = 0uz; i < TILE_COUNT*2; i++)
        if (_walls->atlases[i])
            _walls->mesh_indexes[count++] = uint16_t(i);

    if (count == 0)
        return {wall_mesh, {}, 0};

    std::sort(_walls->mesh_indexes.data(), _walls->mesh_indexes.data() + (ptrdiff_t)count,
              [this](uint16_t a, uint16_t b) {
                  return _walls->atlases[a] < _walls->atlases[b];
              });
#endif

    wall_mesh = make_wall_mesh();
    return { wall_mesh, _walls->mesh_indexes, (size_t)wall_mesh.count()/6u };
}

} // namespace floormat
