#include "chunk.hpp"
#include "tile-bbox.hpp"
#include "quads.hpp"
#include "wall-atlas.hpp"
#include "spritebatch.hpp"
#include "point.inl"
#include "compat/function2.hpp"
#include "RTree-search.hpp"
#include "shaders/shader.hpp"
#include "depth.hpp"
#include "renderer.hpp"
#include "hole-cut.hpp"
#include "loader/loader.hpp"
#include "sprite-atlas.hpp"
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <mg/Range.h>

namespace floormat {

void chunk::ensure_alloc_walls()
{
    if (!_walls) [[unlikely]]
        _walls = Pointer<wall_stuff>{InPlaceInit};
}

namespace {

using Wall::Group;
using Wall::Group_;
using Wall::Direction_;
using Wall::Frame;

struct HoleData
{
    Range2D pos;
    Math::Range1D<uint8_t> z;
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
            arrayAppend(output, { bb, z });
        }, c, wall_bb->first(), wall_bb->second(), can_see_through_mask);
    }

    return output;
}

struct WallFragment
{
    Vector2 remove_from_start_xz, remove_from_end_xz;
    Range2D face_coords;
    Range3D world_coords;
};

Array<Range2D> wall_fragments, next_wall_fragments;

ArrayView<WallFragment> cut_wall_face(Array<WallFragment>& output, ArrayView<const HoleData> holes, local_coords tile_pos, bool IsWest)
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
        const auto hole_slice = Range2D{
            {hole.pos.min()[XAxis], (float)hole.z.min()},
            {hole.pos.max()[XAxis], (float)hole.z.max()},
        };

        arrayResize(next_wall_fragments, 0);

        for (auto x : wall_fragments)
        {
            const auto frags = CutResult<float>::cut(x, hole_slice);

            for (auto i = 0u; i < frags.size; i++)
                arrayAppend(next_wall_fragments, frags.array[i]);
#if 0
            if (frags.s != (uint8_t)-1)
                for (auto i = 0u; i < frags.size; i++)
                    DBG << "good frag" << i << frags.array[i].min() << frags.array[i].max();
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
        w_min[XAxis] = w.min().x();
        w_max[XAxis] = w.max().x();
        w_min[1-XAxis] = -half_tile;
        w_max[1-XAxis] = -half_tile;
        w_min[2] = w.min().y();
        w_max[2] = w.max().y();

        auto frag = WallFragment {
            .remove_from_start_xz = w.min() - bb_min,
            .remove_from_end_xz = bb_max - w.max(),
            .face_coords = { w.min(), w.max() },
            .world_coords = { w_min, w_max },
        };
        arrayAppend(output, frag);
    }

    arrayResize(wall_fragments, 0);
    arrayResize(next_wall_fragments, 0);

    return output;
}

template<Group_ G, bool IsWest>
constexpr int32_t depth_offset_for_group(uint32_t depth)
{
    (void)depth;
    static_assert(G < Group_::COUNT);
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
    auto ret = part_offset;
    return ret;
}

uint32_t variant_index(uint32_t frame_count, global_coords coord, variant_t variant, bool is_west)
{
    if (variant == (variant_t)-1)
        variant = (variant_t)(Vector2ui(coord.raw()).sum() + uint32_t{is_west});
    return variant % frame_count;
}

Array<HoleData> hole_data;

Array<WallFragment> fragdata;

template<Group_ G, bool IsWest>
void do_wall_part(const Group& group, wall_atlas& A, chunk& c, chunk::wall_stuff& W,
                  SpriteList& wsl,
                  global_coords coord, uint32_t tile)
{
    if (!group.is_defined)
        return;

    const uint32_t k = tile*2 + IsWest;
    constexpr auto D = IsWest ? Direction_::W : Direction_::N;
    const auto variant_2 = W.variants[k];
    const auto pos = local_coords{tile};
    const auto center = Vector3(point{c.coord(), pos, {}});
    const auto& dir = A.calc_direction(D);
    const float depth_start = Render::get_status().is_clipdepth01_enabled ? 0.f : -1.f;
    const auto Depth = A.info().depth;
    const auto Depthʹ = (float)(int)Depth;
    const point tile_center {c.coord(), pos, {}};
    constexpr auto half = iTILE_SIZE2/2;
    constexpr float X = (float)half.x(), Y = (float)half.y(), Z = TILE_SIZE.z();

    const auto holes = find_wall_holes_in_world_coords(hole_data, c, pos, IsWest);
    const auto fragments = cut_wall_face(fragdata, holes, pos, IsWest);

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
                const auto sprites = A.sprites(dir.top);
                const auto v2 = variant_index((uint32_t)frames.size(), coord, variant_2, IsWest);
                const auto& frame = frames[v2];
                const auto& sp = sprites[v2];
                fm_assert(frame.size.x() == Depth);
                fm_assert(frame.size.y() >= Depth);
                const auto texcoords = loader.atlas().texcoords_for(
                    sp,
                    Vector2ui{0, frame.size.y() - Depth},
                    Vector2ui{Depth, Depth},
                    dir.top.mirrored);
                const auto depth_offset = depth_offset_for_group<Group_::top, IsWest>(A.depth());
                const Quads::depths depth = {
                    Depth::value_at(depth_start, tile_center + Vector2i{-half.x() - (int)Depth, -half.y() - (int)Depth}, depth_offset),
                    Depth::value_at(depth_start, tile_center + Vector2i{-half.x(), -half.y() - (int)Depth}, depth_offset),
                    Depth::value_at(depth_start, tile_center + Vector2i{-half.x() - (int)Depth, -half.y()}, depth_offset),
                    Depth::value_at(depth_start, tile_center + Vector2i{-half.x(), -half.y()}, depth_offset),
                };
                for (const auto& frag : fragments)
                {
                    if (frag.remove_from_start_xz.x() != 0.f)
                        continue;
                    const float frag_top_z = Z - frag.remove_from_end_xz.y();
                    const Quads::quad quad = {{
                        {-X - Depthʹ, -Y - Depthʹ, frag_top_z},
                        {-X,          -Y - Depthʹ, frag_top_z},
                        {-X - Depthʹ, -Y,          frag_top_z},
                        {-X,          -Y,          frag_top_z},
                    }};
                    Quads::vertexes v;
                    for (uint8_t j = 0; j < 4; j++)
                    {
                        const auto k = Quads::ccw_order[j];
                        v[j] = {quad[k] + center, texcoords[k], depth[k]};
                    }
                    wsl.add(v, depth[0], nullptr);
                }
            }
        }
        if (corner_ok) [[unlikely]]
        {
            const auto depth_offset = depth_offset_for_group<Group_::corner, IsWest>(A.depth());

            Quads::depths depth;
            if constexpr (!IsWest)
                depth = Quads::depth_quad<0, 0, 1, 1>(tile_center + Vector2i{-half.x(), -half.y()},
                                                      tile_center + Vector2i{-half.x() - (int)Depth, -half.y()},
                                                      depth_offset);
            else
                depth = Quads::depth_quad(tile_center + Vector2i{-half.x(), -half.y()},
                                          tile_center + Vector2i{-half.x(), -half.y() - (int)Depth},
                                          depth_offset);

            if (dir.corner.is_defined)
            {
                const auto frames = A.frames(dir.corner);
                const auto sprites = A.sprites(dir.corner);
                const auto v2 = variant_index((uint32_t)frames.size(), coord, variant_2, IsWest);
                const auto& frame = frames[v2];
                const auto& sp = sprites[v2];
                for (const auto& frag : fragments)
                {
                    const auto& rs = frag.remove_from_start_xz;
                    const auto& re = frag.remove_from_end_xz;
                    if (rs.x() != 0.f)
                        continue;
                    const auto sub_size = Vector2ui{frame.size.x(), frame.size.y() - (unsigned)rs.y() - (unsigned)re.y()};
                    if (!sub_size.y())
                        continue;
                    const auto texcoords = loader.atlas().texcoords_for(
                        sp, Vector2ui{0, (unsigned)re.y()}, sub_size, dir.corner.mirrored);
                    const auto frag_zmin = rs.y();
                    const auto frag_zmax = Z - re.y();
                    Quads::quad quad;
                    if constexpr (!IsWest)
                        quad = {
                            {
                                {-X, -Y, frag_zmin},
                                {-X, -Y, frag_zmax},
                                {-X - Depthʹ, -Y, frag_zmin},
                                {-X - Depthʹ, -Y, frag_zmax},
                            }
                        };
                    else
                        quad = {{
                            {-X, -Y - Depthʹ, frag_zmin},
                            {-X, -Y - Depthʹ, frag_zmax},
                            {-X, -Y, frag_zmin},
                            {-X, -Y, frag_zmax},
                        }};
                    Quads::vertexes v;
                    for (uint8_t j = 0; j < 4; j++)
                        v[j] = {quad[j] + center, texcoords[j], depth[j]};
                    wsl.add(v, depth[0], nullptr);
                }
            }
            else if (dir.wall.is_defined) [[likely]]
            {
                const auto frames = A.frames(dir.wall);
                const auto sprites = A.sprites(dir.wall);
                const auto v2 = variant_index((uint32_t)frames.size(), coord, variant_2, IsWest);
                const auto& frame = frames[v2];
                const auto& sp = sprites[v2];
                fm_assert(frame.size.x() > Depth);
                for (const auto& frag : fragments)
                {
                    const auto& rs = frag.remove_from_start_xz;
                    const auto& re = frag.remove_from_end_xz;
                    if (rs.x() != 0.f)
                        continue;
                    const auto sub_size = Vector2ui{Depth, frame.size.y() - (unsigned)rs.y() - (unsigned)re.y()};
                    if (!sub_size.y())
                        continue;
                    const auto texcoords = loader.atlas().texcoords_for(
                        sp,
                        Vector2ui{frame.size.x() - Depth, (unsigned)re.y()},
                        sub_size,
                        dir.wall.mirrored);
                    const auto frag_zmin = rs.y();
                    const auto frag_zmax = Z - re.y();
                    Quads::quad quad;
                    if constexpr (!IsWest)
                        quad = {
                            {
                                {-X, -Y, frag_zmin},
                                {-X, -Y, frag_zmax},
                                {-X - Depthʹ, -Y, frag_zmin},
                                {-X - Depthʹ, -Y, frag_zmax},
                            }
                        };
                    else
                        quad = {{
                            {-X, -Y - Depthʹ, frag_zmin},
                            {-X, -Y - Depthʹ, frag_zmax},
                            {-X, -Y, frag_zmin},
                            {-X, -Y, frag_zmax},
                        }};
                    Quads::vertexes v;
                    for (uint8_t j = 0; j < 4; j++)
                        v[j] = {quad[j] + center, texcoords[j], depth[j]};
                    wsl.add(v, depth[0], nullptr);
                }
            }
        }
    }

    {
        const auto frames = A.frames(group);
        const auto sprites = A.sprites(group);
        const auto v2 = variant_index((uint32_t)frames.size(), coord, variant_2, IsWest);
        const auto& frame = frames[v2];
        const auto& sp = sprites[v2];
        const auto depth_offset = depth_offset_for_group<G, IsWest>(A.depth());

        for (const auto& frag : fragments)
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
                const auto sub_offset_loc = Vector2ui(tex_left, (unsigned)re.y());
                const auto sub_size = frame.size - Vector2ui(tex_left + tex_right, (unsigned)rs.y() + (unsigned)re.y());
                if (!sub_size.x() || !sub_size.y())
                    continue;
                const auto texcoords = loader.atlas().texcoords_for(sp, sub_offset_loc, sub_size, group.mirrored);

                Quads::quad quad;
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

                Quads::depths depth;
                if constexpr (!IsWest)
                    depth = Quads::depth_quad(tile_center + Vector2i{-half.x() + (int)rs.x(), -half.y()},
                                              tile_center + Vector2i{half.x() - (int)re.x(), -half.y()},
                                              depth_offset);
                else
                    depth = Quads::depth_quad(tile_center + Vector2i{-half.x(), half.y() - (int)re.x()},
                                              tile_center + Vector2i{-half.x(), -half.y() + (int)rs.x()},
                                              depth_offset);

                Quads::vertexes v;
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth[j]};
                wsl.add(v, depth[0], nullptr);
            }
            else if constexpr (G == Group_::side)
            {
                const auto sub_offset_loc = Vector2ui(0, (unsigned)re.y());
                const auto sub_size = Vector2ui(frame.size.x(), frame.size.y() - (unsigned)rs.y() - (unsigned)re.y());
                if (!sub_size.y())
                    continue;
                const auto texcoords = loader.atlas().texcoords_for(sp, sub_offset_loc, sub_size, group.mirrored);

                const auto frag_x1 =  X - re.x();
                const auto frag_y1 =  Y - re.x();

                Quads::quad quad;
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

                Quads::depths depth;
                if constexpr (!IsWest)
                    depth = Quads::depth_quad<0,0,1,1>(tile_center + Vector2i{half.x() - (int)re.x(), -half.y() - (int)Depth},
                                                       tile_center + Vector2i{half.x() - (int)re.x(), -half.y()},
                                                       depth_offset);
                else
                    depth = Quads::depth_quad<0,0,1,1>(tile_center + Vector2i{-half.x(), half.y() - (int)re.x()},
                                                       tile_center + Vector2i{-half.x() - (int)Depth, half.y() - (int)re.x()},
                                                       depth_offset);

                Quads::vertexes v;
                for (uint8_t j = 0; j < 4; j++)
                    v[j] = {quad[j] + center, texcoords[j], depth[j]};
                wsl.add(v, depth[0], nullptr);
            }
            else if constexpr (G == Group_::top)
            {
                const auto sub_offset_loc = Vector2ui(0, tex_right);
                const auto sub_size = Vector2ui(frame.size.x(), frame.size.y() - tex_left - tex_right);
                if (!sub_size.y())
                    continue;
                const auto texcoords = loader.atlas().texcoords_for(sp, sub_offset_loc, sub_size, group.mirrored);

                const auto frag_z = Z - re.y();
                Quads::quad quad;
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

                Quads::depths depth;
                if constexpr (!IsWest)
                    depth = {
                        Depth::value_at(depth_start, tile_center + Vector2i{-half.x() + (int)rs.x(), -half.y() - (int)Depth}, depth_offset),
                        Depth::value_at(depth_start, tile_center + Vector2i{half.x() - (int)re.x(), -half.y() - (int)Depth}, depth_offset),
                        Depth::value_at(depth_start, tile_center + Vector2i{-half.x() + (int)rs.x(), -half.y()}, depth_offset),
                        Depth::value_at(depth_start, tile_center + Vector2i{half.x() - (int)re.x(), -half.y()}, depth_offset),
                    };
                else
                    depth = {
                        Depth::value_at(depth_start, tile_center + Vector2i{-half.x(), -half.y() + (int)rs.x()}, depth_offset),
                        Depth::value_at(depth_start, tile_center + Vector2i{-half.x(), half.y() - (int)re.x()}, depth_offset),
                        Depth::value_at(depth_start, tile_center + Vector2i{-half.x() - (int)Depth, -half.y() + (int)rs.x()}, depth_offset),
                        Depth::value_at(depth_start, tile_center + Vector2i{-half.x() - (int)Depth, half.y() - (int)re.x()}, depth_offset),
                    };

                Quads::vertexes v;
                for (uint8_t j = 0; j < 4; j++)
                {
                    const auto k = Quads::ccw_order[j];
                    v[j] = {quad[k] + center, texcoords[k], depth[k]};
                }
                wsl.add(v, depth[0], nullptr);
            }
            else
                static_assert(false);
        }
    }
}

} // namespace

void chunk::ensure_wall_mesh(SpriteBatch& sb)
{
    if (!_walls)
        return;
    if (_walls_modified)
    {
        _walls_modified = false;
        wall_static_mesh.clear();
        auto& W = *_walls;
        for (uint32_t k = 0; k < TILE_COUNT; k++)
        {
            const auto coord = global_coords{_coord, local_coords{k}};

            static_assert(Wall::Group_COUNT == /* 5 */ 4);
            static_assert((int)Direction_::COUNT == 2);

            if (auto* A_nʹ = W.atlases[k*2 + 0].get())
            {
                auto& A_n = *A_nʹ;
                const auto& dir = A_n.calc_direction(Direction_::N);
                do_wall_part<Group_::wall, false>(dir.wall, A_n, *this, W, wall_static_mesh, coord, k);
                do_wall_part<Group_::side, false>(dir.side, A_n, *this, W, wall_static_mesh, coord, k);
                do_wall_part<Group_::top,  false>(dir.top,  A_n, *this, W, wall_static_mesh, coord, k);
            }
            if (auto* A_wʹ = W.atlases[k*2 + 1].get())
            {
                auto& A_w = *A_wʹ;
                const auto& dir = A_w.calc_direction(Direction_::W);
                do_wall_part<Group_::wall,  true>(dir.wall, A_w, *this, W, wall_static_mesh, coord, k);
                do_wall_part<Group_::side,  true>(dir.side, A_w, *this, W, wall_static_mesh, coord, k);
                do_wall_part<Group_::top,   true>(dir.top,  A_w, *this, W, wall_static_mesh, coord, k);
            }
        }
    }
    sb.emit(wall_static_mesh, false);
}

} // namespace floormat
