#include "chunk.hpp"
#include "tile-bbox.hpp"
#include "quads.hpp"
#include "wall-atlas.hpp"
#include "shaders/shader.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/PairStl.h>
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

constexpr Vector2 half_tile = TILE_SIZE2*.5f;
constexpr float X = half_tile.x(), Y = half_tile.y(), Z = TILE_SIZE.z();

using namespace floormat::Quads;
using Wall::Group_;
using Wall::Direction_;

template<Group_ G, bool IsWest> constexpr std::array<Vector3, 4> make_vertex_data(float depth);

// -----------------------

// corner left
template<> quad constexpr make_vertex_data<Group_::corner_L, false>(float)
{
    constexpr float x_offset = (float)(unsigned)X;
    return {{
        { -X + x_offset, -Y, Z },
        { -X + x_offset, -Y, 0 },
        { -X, -Y, Z },
        { -X, -Y, 0 },
    }};
}

// corner right
template<> quad constexpr make_vertex_data<Group_::corner_R, true>(float)
{
    constexpr float y_offset = TILE_SIZE.y() - (float)(unsigned)Y;
    return {{
        {-X,  -Y, Z },
        {-X,  -Y, 0 },
        {-X,  -Y + y_offset, Z },
        {-X,  -Y + y_offset, 0 },
    }};
}

// wall north
template<> quad constexpr make_vertex_data<Group_::wall, false>(float)
{
    return {{
        { X, -Y, Z },
        { X, -Y, 0 },
        {-X, -Y, Z },
        {-X, -Y, 0 },
    }};
}

// wall west
template<> quad constexpr make_vertex_data<Group_::wall, true>(float)
{
    return {{
        {-X, -Y, Z },
        {-X, -Y, 0 },
        {-X,  Y, Z },
        {-X,  Y, 0 },
    }};
}

// side north
template<> quad constexpr make_vertex_data<Group_::side, false>(float depth)
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

// side west
template<> quad constexpr make_vertex_data<Group_::side, true>(float depth)
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

// top north
template<> quad constexpr make_vertex_data<Group_::top, false>(float depth)
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

// top west
template<> quad constexpr make_vertex_data<Group_::top, true>(float depth)
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

#define FM_WALL_MAKE_CASE(name) \
    case name: return make_vertex_data<name, IsWest>(depth)

#define FM_WALL_MAKE_CASES() \
    do {                                                        \
        switch (G)                                              \
        {                                                       \
            FM_WALL_MAKE_CASE(Group_::wall);                    \
            FM_WALL_MAKE_CASE(Group_::side);                    \
            FM_WALL_MAKE_CASE(Group_::top);                     \
            FM_WALL_MAKE_CASE(Group_::corner_L);                \
            FM_WALL_MAKE_CASE(Group_::corner_R);                \
            case Group_::COUNT:                                 \
                fm_abort("invalid wall group '%d'", (int)G);    \
        }                                                       \
    } while (false)

quad get_vertex_data(Direction_ D, Group_ G, float depth)
{
    CORRADE_ASSUME(G < Group_::COUNT);
    CORRADE_ASSUME(D < Direction_::COUNT);

    switch (D)
    {
    case Direction_::COUNT:
        fm_abort("invalid wall direction '%d'", (int)D);
    case Direction_::N: {
        constexpr auto IsWest = false;
        FM_WALL_MAKE_CASES();
        break;
    }
    case Direction_::W: {
        constexpr auto IsWest = true;
        FM_WALL_MAKE_CASES();
    }
    }
}

// -----------------------

Array<Quads::indexes> make_indexes_()
{
    auto array = Array<Quads::indexes>{NoInit, chunk::max_wall_mesh_size};
    for (auto i = 0uz; i < chunk::max_wall_mesh_size; i++)
        array[i] = quad_indexes(i);
    return array;
}

ArrayView<const Quads::indexes> make_indexes(size_t max)
{
    static const auto indexes = make_indexes_();
    fm_assert(max < chunk::max_wall_mesh_size);
    return indexes.prefix(max);
}

} // namespace

GL::Mesh chunk::make_wall_mesh(size_t count)
{
    fm_debug_assert(_walls);
    fm_debug_assert(count > 0);
    //std::array<std::array<vertex, 4>, TILE_COUNT*2> vertexes;
    //vertex vertexes[TILE_COUNT*2][4];
    uint32_t i = 0, N = 0;

    static auto vertexes = Array<vertex>{NoInit, max_wall_mesh_size};

    for (auto k = 0uz; k < count; k++)
    {
        const auto i = _walls->mesh_indexes[k];
        const auto& atlas = _walls->atlases[i];
        fm_assert(atlas != nullptr);
        const auto variant = _walls->variants[i];
        const local_coords pos{i / 2u};
        const auto center = Vector3(pos) * TILE_SIZE;
        const auto& dir = atlas->calc_direction(i & 1 ? Wall::Direction_::W : Wall::Direction_::N);
        for (auto [_, member, group] : Wall::Direction::groups)
        {
            const auto& G = dir.*member;
            if (!G.is_defined)
                continue;

        }
        // ...

        //const auto quad = i & 1 ? wall_quad_W(center, TILE_SIZE) : wall_quad_N(center, TILE_SIZE);
        const float depth = tile_shader::depth_value(pos, tile_shader::wall_depth_offset);
        //const auto texcoords = atlas->texcoords_for_id(variant);
        auto& v = vertexes[N++];
        for (auto j = 0uz; j < 4; j++)
            v[j] = { quad[j], texcoords[j], depth, };
    }

    auto vertex_view = vertexes.prefix(N);
    auto index_view = make_indexes(N);

    //auto indexes = make_index_array<2>(count);
    //const auto vertex_view = ArrayView{&vertexes[0], count};
    //const auto vert_index_view = ArrayView{indexes.data(), count};

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{vert_index_view}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(int32_t(6 * count));
    return mesh;
}

auto chunk::ensure_wall_mesh() noexcept -> wall_mesh_tuple
{
    if (!_walls)
        return {wall_mesh, {}, 0};

    if (!_walls_modified)
        return { wall_mesh, _walls->mesh_indexes, size_t(wall_mesh.count()/6) };
    _walls_modified = false;

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

    wall_mesh = make_wall_mesh(count);
    return { wall_mesh, _walls->mesh_indexes, count };
}

} // namespace floormat
