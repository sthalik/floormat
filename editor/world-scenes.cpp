#include "app.hpp"
#include "src/world.hpp"
#include "src/hole.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/wall-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "compat/borrowed-ptr.inl"
#include "floormat/main.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"

namespace floormat {

void app::populate_hole_stress_test()
{
    reset_world();
    auto& w = M->world();

    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];
    c.mark_modified();

    auto wall   = loader.wall_atlas("test2", loader_policy::warn);
    auto ground = loader.ground_atlas("floor-tiles");

    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };

    auto place = [&](local_coords at, Vector2b off, Vector2ub size,
                     u8 z_off = 0, u8 ht = tile_size_z) {
        hole_proto p;
        p.z_offset    = z_off;
        p.height      = ht;
        p.bbox_size   = size;
        p.bbox_offset = off;
        p.pass        = pass_mode::pass;
        w.make_object<hole>(w.make_id(), {ch, at}, p);
    };

    for (u8 x = 0; x < 12; x++)
        c[{x, 1}].wall_north() = {wall, 0};
    place({0,1},  {-7*tile_size_xy/16, -tile_size_xy/2}, {tile_size_xy/8,   tile_size_xy});
    place({1,1},  {-3*tile_size_xy/8,  -tile_size_xy/2}, {tile_size_xy/4,   tile_size_xy});
    place({2,1},  {-tile_size_xy/4,    -tile_size_xy/2}, {tile_size_xy/2,   tile_size_xy});
    place({3,1},  {-tile_size_xy/8,    -tile_size_xy/2}, {3*tile_size_xy/4, tile_size_xy});
    place({4,1},  { 0,                 -tile_size_xy/2}, {tile_size_xy,     tile_size_xy});
    place({5,1},  { tile_size_xy/8,    -tile_size_xy/2}, {3*tile_size_xy/4, tile_size_xy});
    place({6,1},  { tile_size_xy/4,    -tile_size_xy/2}, {tile_size_xy/2,   tile_size_xy});
    place({7,1},  { 3*tile_size_xy/8,  -tile_size_xy/2}, {tile_size_xy/4,   tile_size_xy});
    place({8,1},  { 7*tile_size_xy/16, -tile_size_xy/2}, {tile_size_xy/8,   tile_size_xy});
    place({9,1},  { 0,                 -tile_size_xy/2}, {3*tile_size_xy/8, tile_size_xy});
    place({10,1}, { 0,                 -tile_size_xy/2}, {tile_size_xy/8,   tile_size_xy});
    place({11,1}, {-7*tile_size_xy/16, -tile_size_xy/2}, {tile_size_xy/8,   tile_size_xy});
    place({11,1}, { 7*tile_size_xy/16, -tile_size_xy/2}, {tile_size_xy/8,   tile_size_xy});

    for (u8 x = 0; x < 10; x++)
        c[{x, 3}].wall_north() = {wall, 0};
    place({0,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, 0,               tile_size_z/4);
    place({1,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, 0,               tile_size_z/2);
    place({2,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, 0,               3*tile_size_z/4);
    place({3,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, 0,               tile_size_z);
    place({4,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, tile_size_z/4,   3*tile_size_z/4);
    place({5,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, tile_size_z/2,   tile_size_z/2);
    place({6,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, 3*tile_size_z/4, tile_size_z/4);
    place({7,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, tile_size_z/3,   tile_size_z/3);
    place({8,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, 2*tile_size_z/3, tile_size_z/3);
    place({9,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, 0,               tile_size_z/4);
    place({9,3}, {0,-tile_size_xy/2}, {tile_size_xy,tile_size_xy}, 3*tile_size_z/4, tile_size_z/4);

    for (u8 x = 0; x < 3; x++)
        for (u8 y = 5; y < 8; y++) {
            c[{x,y}].wall_north() = {wall, 0};
            c[{x,y}].wall_west()  = {wall, 0};
        }
    place({0,5}, {-tile_size_xy/4, -tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});
    place({1,5}, { 0,              -tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});
    place({2,5}, { tile_size_xy/4, -tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});
    place({0,6}, {-tile_size_xy/4,  0             }, {tile_size_xy/2, tile_size_xy/2});
    place({1,6}, { 0,               0             }, {tile_size_xy/2, tile_size_xy/2});
    place({2,6}, { tile_size_xy/4,  0             }, {tile_size_xy/2, tile_size_xy/2});
    place({0,7}, {-tile_size_xy/4,  tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});
    place({1,7}, { 0,               tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});
    place({2,7}, { tile_size_xy/4,  tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});

    for (u8 y = 0; y < 10; y++)
        c[{13,y}].wall_west() = {wall, 0};
    place({13,0}, {-tile_size_xy/2, -7*tile_size_xy/16}, {tile_size_xy, tile_size_xy/8  });
    place({13,1}, {-tile_size_xy/2, -3*tile_size_xy/8 }, {tile_size_xy, tile_size_xy/4  });
    place({13,2}, {-tile_size_xy/2, -tile_size_xy/4   }, {tile_size_xy, tile_size_xy/2  });
    place({13,3}, {-tile_size_xy/2, -tile_size_xy/8   }, {tile_size_xy, 3*tile_size_xy/4});
    place({13,4}, {-tile_size_xy/2,  0                }, {tile_size_xy, tile_size_xy    });
    place({13,5}, {-tile_size_xy/2,  tile_size_xy/8   }, {tile_size_xy, 3*tile_size_xy/4});
    place({13,6}, {-tile_size_xy/2,  tile_size_xy/4   }, {tile_size_xy, tile_size_xy/2  });
    place({13,7}, {-tile_size_xy/2,  3*tile_size_xy/8 }, {tile_size_xy, tile_size_xy/4  });
    place({13,8}, {-tile_size_xy/2,  7*tile_size_xy/16}, {tile_size_xy, tile_size_xy/8  });
    place({13,9}, {-tile_size_xy/2,  0                }, {tile_size_xy, 3*tile_size_xy/8});

    for (u8 y = 0; y < 8; y++)
        c[{14,y}].wall_west() = {wall, 0};
    place({14,0}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, 0,               tile_size_z/4);
    place({14,1}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, 0,               tile_size_z/2);
    place({14,2}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, 0,               tile_size_z);
    place({14,3}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, tile_size_z/2,   tile_size_z/2);
    place({14,4}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, 3*tile_size_z/4, tile_size_z/4);
    place({14,5}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, tile_size_z/3,   tile_size_z/3);
    place({14,6}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, 0,               tile_size_z/3);
    place({14,6}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, 2*tile_size_z/3, tile_size_z/3);
    place({14,7}, {-tile_size_xy/2,0}, {tile_size_xy,tile_size_xy}, tile_size_z/6,   2*tile_size_z/3);

    for (u8 x = 0; x < 6; x++) {
        c[{x, 9}].wall_north() = {wall, 0};
        place({x, 9}, {0, -tile_size_xy/2}, {tile_size_xy, tile_size_xy},
              u8(x * tile_size_z/6), tile_size_z/6);
    }

    c[{7,9}].wall_north() = {wall, 0};
    c[{8,9}].wall_north() = {wall, 0};
    place({7,9}, {0,-tile_size_xy/2}, {tile_size_xy,   tile_size_xy}, 0,             tile_size_z/2);
    place({7,9}, {0,-tile_size_xy/2}, {tile_size_xy/4, tile_size_xy}, tile_size_z/2, tile_size_z/2);
    place({8,9}, {0,-tile_size_xy/2}, {tile_size_xy/4, tile_size_xy}, 0,             tile_size_z/2);
    place({8,9}, {0,-tile_size_xy/2}, {tile_size_xy,   tile_size_xy}, tile_size_z/2, tile_size_z/2);

    c[{10,9}].wall_north() = {wall, 0};
    c[{11,9}].wall_north() = {wall, 0};
    place({10,9}, {-tile_size_xy/8,    -tile_size_xy/2}, {3*tile_size_xy/4, tile_size_xy});
    place({10,9}, { tile_size_xy/4,    -tile_size_xy/2}, {tile_size_xy/2,   tile_size_xy});
    place({10,9}, {-5*tile_size_xy/16, -tile_size_xy/2}, {tile_size_xy/4,   tile_size_xy});
    place({11,9}, {-tile_size_xy/4, -tile_size_xy/2}, {tile_size_xy/2, tile_size_xy}, 0,               tile_size_z/3);
    place({11,9}, { 0,              -tile_size_xy/2}, {tile_size_xy/2, tile_size_xy}, tile_size_z/3,   tile_size_z/3);
    place({11,9}, { tile_size_xy/4, -tile_size_xy/2}, {tile_size_xy/2, tile_size_xy}, 2*tile_size_z/3, tile_size_z/3);

    for (u8 x = 0; x < 8; x++) {
        c[{x, 11}].wall_north() = {wall, 0};
        auto cx = i8(-7*tile_size_xy/16 + x * tile_size_xy/8);
        place({x, 11}, {cx, -tile_size_xy/2}, {tile_size_xy/4, tile_size_xy});
    }

    for (u8 x = 8; x < 13; x++) {
        c[{x, 11}].wall_west() = {wall, 0};
        int step = x - 8;
        place({x, 11}, {-tile_size_xy/2, 0}, {tile_size_xy, tile_size_xy},
              u8(step * tile_size_z/6),
              u8(tile_size_z - step * tile_size_z/6));
    }

    for (u8 x = 4; x < 8; x++)
        for (u8 y = 5; y < 8; y++) {
            c[{x,y}].wall_north() = {wall, 0};
            c[{x,y}].wall_west()  = {wall, 0};
        }
    place({4,5}, {-tile_size_xy/4, -tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});
    place({5,5}, { 0,              -tile_size_xy/2}, {tile_size_xy, tile_size_xy}, 0,             tile_size_z/2);
    place({5,5}, {-tile_size_xy/2,  0             }, {tile_size_xy, tile_size_xy}, tile_size_z/2, tile_size_z/2);
    place({6,5}, {-tile_size_xy/8,    -tile_size_xy/2}, {3*tile_size_xy/4, tile_size_xy});
    place({6,5}, { tile_size_xy/4,    -tile_size_xy/2}, {tile_size_xy/2,   tile_size_xy});
    place({6,5}, {-5*tile_size_xy/16, -tile_size_xy/2}, {tile_size_xy/4,   tile_size_xy});
    place({7,5}, {-3*tile_size_xy/8,  -tile_size_xy/2},  {tile_size_xy/8, tile_size_xy},   0,               tile_size_z/3);
    place({7,5}, { tile_size_xy/8,    -tile_size_xy/2},  {tile_size_xy/4, tile_size_xy},   tile_size_z/3,   tile_size_z/3);
    place({7,5}, {-7*tile_size_xy/16, -tile_size_xy/8},  {tile_size_xy/8, tile_size_xy/4}, 2*tile_size_z/3, tile_size_z/3);
    place({7,5}, {-7*tile_size_xy/16,  tile_size_xy/4},  {tile_size_xy/8, tile_size_xy/4}, 0,               tile_size_z/2);
    place({4,6}, {0,-tile_size_xy/2}, {tile_size_xy,   tile_size_xy}, 0,               tile_size_z/4);
    place({4,6}, {0,-tile_size_xy/2}, {tile_size_xy/4, tile_size_xy}, tile_size_z/4,   tile_size_z/2);
    place({4,6}, {0,-tile_size_xy/2}, {tile_size_xy,   tile_size_xy}, 3*tile_size_z/4, tile_size_z/4);
    place({5,6}, {-tile_size_xy/4,  -tile_size_xy/2}, {tile_size_xy/2, tile_size_xy});
    place({5,6}, { tile_size_xy/4,  -tile_size_xy/2}, {tile_size_xy/2, tile_size_xy}, tile_size_z/3, tile_size_z/3);
    place({5,6}, {-tile_size_xy/2,  -tile_size_xy/4}, {tile_size_xy,   tile_size_xy/2});
    place({6,6}, {-3*tile_size_xy/8, -tile_size_xy/2}, {tile_size_xy/4, tile_size_xy}, 0,               tile_size_z/3);
    place({6,6}, { 0,                -tile_size_xy/2}, {tile_size_xy/4, tile_size_xy}, tile_size_z/3,   tile_size_z/3);
    place({6,6}, { tile_size_xy/4,   -tile_size_xy/2}, {tile_size_xy/4, tile_size_xy}, 2*tile_size_z/3, tile_size_z/3);
    place({7,6}, {-tile_size_xy/4, -tile_size_xy/2}, {tile_size_xy/2, tile_size_xy}, 0,             tile_size_z/2);
    place({7,6}, { tile_size_xy/4, -tile_size_xy/2}, {tile_size_xy/2, tile_size_xy}, tile_size_z/2, tile_size_z/2);
    place({7,6}, { 0,              -tile_size_xy/2}, {tile_size_xy/4, tile_size_xy}, tile_size_z/4, tile_size_z/2);
    place({7,6}, {-tile_size_xy/2, -tile_size_xy/4}, {tile_size_xy,   tile_size_xy/2});
    place({7,6}, {-tile_size_xy/2,  tile_size_xy/4}, {tile_size_xy,   tile_size_xy/2}, tile_size_z/4, tile_size_z/2);
    place({4,7}, { 0,              -tile_size_xy/2}, {tile_size_xy, tile_size_xy}, 0,               tile_size_z/3);
    place({4,7}, {-tile_size_xy/2,  0             }, {tile_size_xy, tile_size_xy}, 2*tile_size_z/3, tile_size_z/3);
    place({5,7}, {-tile_size_xy/4, -tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2}, 0,             tile_size_z/2);
    place({5,7}, { tile_size_xy/4,  tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2}, tile_size_z/2, tile_size_z/2);
    place({6,7}, { 0,              -tile_size_xy/2}, {tile_size_xy,   tile_size_xy});
    place({6,7}, {-tile_size_xy/2,  0             }, {tile_size_xy,   tile_size_xy});
    place({6,7}, { 0,               0             }, {tile_size_xy/2, tile_size_xy/2}, tile_size_z/4, tile_size_z/2);
    place({6,7}, { tile_size_xy/4,  tile_size_xy/4}, {tile_size_xy/4, tile_size_xy/4});
    place({7,7}, { tile_size_xy/4, -tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});
    place({7,7}, {-tile_size_xy/4,  tile_size_xy/4}, {tile_size_xy/2, tile_size_xy/2});
}

void app::populate_labyrinth()
{
    reset_world();
    auto& w = M->world();

    auto wall   = loader.wall_atlas("test1", loader_policy::warn);
    auto ground = loader.ground_atlas("floor-tiles");

    constexpr int cols = 5, rows = 8;
    constexpr int W = cols * 16, H = rows * 16;
    constexpr int step = 4;

    for (i16 cx = 0; cx < cols; cx++)
        for (i16 cy = 0; cy < rows; cy++) {
            auto& c = w[{cx, cy, 0}];
            for (auto k = 0u; k < TILE_COUNT; k++)
                c[k].ground() = {ground, variant_t(k % ground->num_tiles())};
            c.mark_modified();
        }

    auto GN = [&](int gx, int gy) {
        const auto g = global_coords{gx, gy, 0};
        w[g.chunk3()][g.local()].wall_north() = {wall, 0};
    };
    auto GW = [&](int gx, int gy) {
        const auto g = global_coords{gx, gy, 0};
        w[g.chunk3()][g.local()].wall_west() = {wall, 0};
    };

    for (int x = step; x < W; x++) GN(x, 0);
    for (int x = step; x < W; x++) GN(x, H);
    for (int y = 0; y < H; y++) GW(0, y);
    for (int y = 0; y < H; y++) GW(W, y);

    for (int wy = step; wy < H; wy += step) {
        int gap = ((wy/step - 1) % 2 == 0) ? W - step : 0;
        for (int x = 0; x < W; x++)
            if (x < gap || x >= gap + step)
                GN(x, wy);
    }
}

void app::populate_raycast_fractal()
{
    reset_world();
    auto& w = M->world();

    auto wall1  = loader.wall_atlas("test1", loader_policy::warn);
    auto wall2  = loader.wall_atlas("concrete1", loader_policy::warn);
    auto ground = loader.ground_atlas("floor-tiles");

    constexpr i16 cmin = -10, cmax = 10;

    auto hash2 = [](u32 x, u32 y) -> u32 {
        u32 h = x * 0x9E3779B1u + y * 0x85EBCA77u;
        h ^= h >> 16; h *= 0xC2B2AE3Du;
        h ^= h >> 13; h *= 0x27D4EB2Du;
        h ^= h >> 16;
        return h;
    };

    for (i16 cx = cmin; cx <= cmax; cx++)
        for (i16 cy = cmin; cy <= cmax; cy++)
        {
            auto& ch = w[{cx, cy, 0}];

            for (auto k = 0u; k < TILE_COUNT; k++)
                ch[k].ground() = { ground, variant_t(k % ground->num_tiles()) };

            for (u8 lx = 0; lx < 16; lx++)
                for (u8 ly = 0; ly < 16; ly++)
                {
                    u32 fx = (u32)(cx + 16) * 16 + lx;
                    u32 fy = (u32)(cy + 16) * 16 + ly;

                    if ((hash2(fx,             fy ^ 0xA5A5u) & 0x1Fu) == 0u)
                        ch[{lx, ly}].wall_north() = { wall1, 0 };
                    if ((hash2(fx ^ 0x5A5Au,   fy)           & 0x1Fu) == 0u)
                        ch[{lx, ly}].wall_west()  = { wall2, 0 };
                }

            ch.mark_modified();
        }
}

void app::populate_sweep_aabb_slit()
{
    reset_world();
    auto& w = M->world();

    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];

    auto ground = loader.ground_atlas("floor-tiles");
    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };

    // Edge contact only at spawn; strict-< overlap finds nothing, swept-AABB does.
    auto add_pillar = [&](local_coords tile, Vector2b bb_off) {
        scenery_proto p;
        p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
        p.subtype     = generic_scenery_proto{};
        p.bbox_offset = bb_off;
        p.bbox_size   = Vector2ub{2, 2};
        p.pass        = pass_mode::blocked;
        w.make_scenery(w.make_id(), {ch, tile}, move(p));
    };
    add_pillar({0, 0}, { 0, -1});
    add_pillar({1, 1}, { 1,  0});

    auto C = ensure_player_character(w);
    auto i = C->index();
    C->teleport_to(i, global_coords{ch, {1, 1}}, Vector2b{-32, -32}, rotation_COUNT);

    c.mark_modified();
}

// Arrow keys only: A* keys the pass grid on max(bbox), so it routes this 255x32 critter as
// 255x255 and finds nothing. Stools bound the legs, since a wall's depth strip lies outside its
// tile and narrows the gap.
void app::populate_l_corridor()
{
    reset_world();
    auto& w = M->world();

    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];

    constexpr u8 bbox_long = 255, bbox_short = tile_size_xy/2;
    constexpr u8 room_x0 = 6, room_x1 = 11, room_y0 = 3, room_y1 = 10;
    constexpr u8 leg_w_y = 7, leg_w_x0 = 1, leg_w_x1 = 5;
    // Held SE slides east along the room's south wall, so the leg has to be on the east edge.
    // Centered, it would stall in the corner instead.
    constexpr u8 leg_s_x = room_x1, leg_s_y0 = 11, leg_s_y1 = 14;
    constexpr local_coords start{3, leg_w_y};

    // Held NE parks the box in the room's north-east corner, 16 px from the north wall, where
    // the 255-tall rotated one would stick 111 px out. Two tiles of clearance cut into the wall
    // there receive it; a narrower cut wouldn't span the park spot, a wider one would let the
    // 255-wide box climb in.
    constexpr u8 notch_x0 = room_x1-2, notch_x1 = room_x1-1;
    constexpr u8 notch_y0 = room_y0-2, notch_y1 = room_y0-1;

    auto walkable = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < (int)TILE_MAX_DIM && y < (int)TILE_MAX_DIM &&
               ((y == leg_w_y && x >= leg_w_x0 && x <= leg_w_x1) ||
                (x == leg_s_x && y >= leg_s_y0 && y <= leg_s_y1) ||
                (x >= room_x0 && x <= room_x1 && y >= room_y0 && y <= room_y1) ||
                (x >= notch_x0 && x <= notch_x1 && y >= notch_y0 && y <= notch_y1));
    };

    auto floor1 = loader.ground_atlas("floor-tiles");
    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { floor1, variant_t(k % floor1->num_tiles()) };

    auto stool = loader.scenery("stool1");
    stool.bbox_offset = {};
    stool.bbox_size = Vector2ub(tile_size_xy);
    stool.pass = pass_mode::blocked;

    for (int y = 0; y < (int)TILE_MAX_DIM; y++)
        for (int x = 0; x < (int)TILE_MAX_DIM; x++)
        {
            if (walkable(x, y))
                continue;
            // One ring is enough: the bbox can't overlap a blocker, so it can't reach behind one.
            bool touches = false;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    touches |= walkable(x + dx, y + dy);
            if (touches)
                w.make_scenery(w.make_id(), {ch, local_coords{(u8)x, (u8)y}}, scenery_proto(stool));
        }

    auto C = ensure_player_character(w);
    auto i = C->index();
    C->teleport_to(i, global_coords{ch, start}, Vector2b{}, rotation::NE);
    C->set_bbox({}, {}, Vector2ub{bbox_long, bbox_short}, C->pass);
    C->speed = 4;
    C->delta = 0;

    c.mark_modified();
}

} // namespace floormat
