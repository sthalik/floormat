#include "app.hpp"
#include "src/world.hpp"
#include "src/hole.hpp"
#include "src/wall-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "compat/borrowed-ptr.inl"
#include "floormat/main.hpp"
#include "loader/loader.hpp"

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
                     uint8_t z_off = 0, uint8_t ht = tile_size_z) {
        hole_proto p;
        p.z_offset    = z_off;
        p.height      = ht;
        p.bbox_size   = size;
        p.bbox_offset = off;
        p.pass        = pass_mode::pass;
        w.make_object<hole>(w.make_id(), {ch, at}, p);
    };

    for (uint8_t x = 0; x < 12; x++)
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

    for (uint8_t x = 0; x < 10; x++)
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

    for (uint8_t x = 0; x < 3; x++)
        for (uint8_t y = 5; y < 8; y++) {
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

    for (uint8_t y = 0; y < 10; y++)
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

    for (uint8_t y = 0; y < 8; y++)
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

    for (uint8_t x = 0; x < 6; x++) {
        c[{x, 9}].wall_north() = {wall, 0};
        place({x, 9}, {0, -tile_size_xy/2}, {tile_size_xy, tile_size_xy},
              uint8_t(x * tile_size_z/6), tile_size_z/6);
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

    for (uint8_t x = 0; x < 8; x++) {
        c[{x, 11}].wall_north() = {wall, 0};
        auto cx = int8_t(-7*tile_size_xy/16 + x * tile_size_xy/8);
        place({x, 11}, {cx, -tile_size_xy/2}, {tile_size_xy/4, tile_size_xy});
    }

    for (uint8_t x = 8; x < 13; x++) {
        c[{x, 11}].wall_west() = {wall, 0};
        int step = x - 8;
        place({x, 11}, {-tile_size_xy/2, 0}, {tile_size_xy, tile_size_xy},
              uint8_t(step * tile_size_z/6),
              uint8_t(tile_size_z - step * tile_size_z/6));
    }

    for (uint8_t x = 4; x < 8; x++)
        for (uint8_t y = 5; y < 8; y++) {
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

    for (int16_t cx = 0; cx < cols; cx++)
        for (int16_t cy = 0; cy < rows; cy++) {
            auto& c = w[{cx, cy, 0}];
            for (auto k = 0u; k < TILE_COUNT; k++)
                c[k].ground() = {ground, variant_t(k % ground->num_tiles())};
            c.mark_modified();
        }

    auto GN = [&](int gx, int gy) {
        w[{int16_t(gx/16), int16_t(gy/16), 0}][{uint8_t(gx%16), uint8_t(gy%16)}].wall_north() = {wall, 0};
    };
    auto GW = [&](int gx, int gy) {
        w[{int16_t(gx/16), int16_t(gy/16), 0}][{uint8_t(gx%16), uint8_t(gy%16)}].wall_west() = {wall, 0};
    };

    for (int x = step; x < W; x++) GN(x, 0);
    for (int x = step; x < W; x++)
        w[{int16_t(x/16), int16_t(rows), 0}][{uint8_t(x%16), 0}].wall_north() = {wall, 0};
    for (int y = 0; y < H; y++) GW(0, y);
    for (int cy = 0; cy < rows; cy++)
        for (int y = 0; y < 16; y++)
            w[{int16_t(cols), int16_t(cy), 0}][{0, uint8_t(y)}].wall_west() = {wall, 0};

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

    constexpr int16_t cmin = -10, cmax = 10;

    auto hash2 = [](uint32_t x, uint32_t y) -> uint32_t {
        uint32_t h = x * 0x9E3779B1u + y * 0x85EBCA77u;
        h ^= h >> 16; h *= 0xC2B2AE3Du;
        h ^= h >> 13; h *= 0x27D4EB2Du;
        h ^= h >> 16;
        return h;
    };

    for (int16_t cx = cmin; cx <= cmax; cx++)
        for (int16_t cy = cmin; cy <= cmax; cy++)
        {
            auto& ch = w[{cx, cy, 0}];

            for (auto k = 0u; k < TILE_COUNT; k++)
                ch[k].ground() = { ground, variant_t(k % ground->num_tiles()) };

            for (uint8_t lx = 0; lx < 16; lx++)
                for (uint8_t ly = 0; ly < 16; ly++)
                {
                    uint32_t fx = (uint32_t)(cx + 16) * 16 + lx;
                    uint32_t fy = (uint32_t)(cy + 16) * 16 + ly;

                    if ((hash2(fx,             fy ^ 0xA5A5u) & 0x1Fu) == 0u)
                        ch[{lx, ly}].wall_north() = { wall1, 0 };
                    if ((hash2(fx ^ 0x5A5Au,   fy)           & 0x1Fu) == 0u)
                        ch[{lx, ly}].wall_west()  = { wall2, 0 };
                }

            ch.mark_modified();
        }
}

void app::maybe_initialize_chunk_([[maybe_unused]] const chunk_coords_& pos, chunk& c)
{
    auto floor1 = loader.ground_atlas("floor-tiles");

    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { floor1, variant_t(k % floor1->num_tiles()) };
    c.mark_modified();
}

void app::maybe_initialize_chunk([[maybe_unused]] const chunk_coords_& pos, [[maybe_unused]] chunk& c)
{
    //maybe_initialize_chunk_(pos, c);
}

} // namespace floormat
