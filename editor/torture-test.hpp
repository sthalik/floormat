#pragma once
#include "src/scenery-proto.hpp"
#include "compat/array-size.hpp"
#include "compat/borrowed-ptr.h"
#include <array>
#include <mg/Vector4.h>

namespace floormat { class world; class ground_atlas; class wall_atlas; }

// The torture test scene, shared by the editor and floormat-benchmark.
namespace floormat::torture_test {

constexpr inline i16 bench_chunk_min = -8, bench_chunk_max = 8;

// Prime count so the color never lines up with any of the placement periods.
constexpr inline Vector4ub light_colors[] = {
    { 255,  64,  64, 255 }, { 255, 128,  32, 255 }, { 255, 192,  32, 255 },
    { 255, 255,  64, 255 }, { 192, 255,  64, 255 }, {  96, 255,  64, 255 },
    {  64, 255, 144, 255 }, {  64, 255, 224, 255 }, {  64, 224, 255, 255 },
    {  64, 160, 255, 255 }, {  64,  96, 255, 255 }, { 128,  64, 255, 255 },
    { 192,  64, 255, 255 }, { 255,  64, 224, 255 }, { 255,  64, 144, 255 },
    { 255, 224, 192, 255 }, { 192, 224, 255, 255 }, { 255, 255, 255, 255 },
    { 224, 160,  96, 255 }, { 160, 224, 160, 255 }, { 160, 160, 224, 255 },
    { 224, 224, 128, 255 }, { 224, 128, 160, 255 },
};

static_assert(array_size(light_colors) == 23);

constexpr u32 pmod(i32 x, i32 m)
{
    auto r = x % m;
    return u32(r < 0 ? r + m : r);
}

inline u32 hash2(u32 x, u32 y) // same mixer as app::populate_raycast_fractal()
{
    u32 h = x * 0x9E3779B1u + y * 0x85EBCA77u;
    h ^= h >> 16; h *= 0xC2B2AE3Du;
    h ^= h >> 13; h *= 0x27D4EB2Du;
    h ^= h >> 16;
    return h;
}

constexpr inline const char* const scenery_names[] = {
    "chair1", "stool1", "drawers1", "shelf1", "table4", "door1", "control panel (wall) 1",
};

struct scene_assets
{
    std::array<bptr<ground_atlas>, 2> ground;
    std::array<bptr<wall_atlas>, 3> walls;
    std::array<scenery_proto, array_size(scenery_names)> scenery;
};

scene_assets load_assets();

void generate_scene(world& w, int z_min, int z_max, bool walls,
                    i16 cmin = bench_chunk_min, i16 cmax = bench_chunk_max);

} // namespace floormat::torture_test
