#include "src/raycast-diag.hpp"
#include "src/world.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include <benchmark/benchmark.h>
#include <Magnum/Math/Functions.h>

namespace floormat {

namespace {

world make_world()
{
    constexpr auto var = (variant_t)-1;
    auto wall1_ = loader.wall_atlas("test1"_s);
    auto wall2_ = loader.wall_atlas("concrete1"_s);
    auto wall1 = wall_image_proto{wall1_, var};
    auto wall2 = wall_image_proto{wall2_, var};

    auto w = world{};
    w[global_coords{{0, 3, 0}, {15,  0}}].t.wall_north() = wall1;
    w[global_coords{{1, 3, 0}, { 0,  0}}].t.wall_north() = wall1;
    w[global_coords{{1, 3, 0}, { 0,  0}}].t.wall_north() = wall1;
    w[global_coords{{1, 2, 0}, { 1, 15}}].t.wall_west()  = wall1;
    w[global_coords{{1, 2, 0}, { 1, 14}}].t.wall_west()  = wall1;

    w[global_coords{{0, 1, 0}, { 8, 11}}].t.wall_west()  = wall2;
    w[global_coords{{0, 1, 0}, { 8, 10}}].t.wall_west()  = wall2;
    w[global_coords{{0, 1, 0}, { 7, 10}}].t.wall_north() = wall2;
    w[global_coords{{0, 1, 0}, { 6, 10}}].t.wall_north() = wall2;

    w[global_coords{{0, 1, 0}, { 9,  8}}].t.wall_north() = wall1;
    w[global_coords{{0, 1, 0}, {10,  8}}].t.wall_north() = wall1;
    w[global_coords{{0, 1, 0}, {11,  8}}].t.wall_west()  = wall1;

    w[global_coords{{0, 2, 0}, { 9,  0}}].t.wall_north() = wall1;
    w[global_coords{{0, 2, 0}, {10,  0}}].t.wall_north() = wall1;

    for (int16_t k = -5; k <= -1; k++)
    {
        auto& ch = w[chunk_coords_{-5, -5, 0}];
        for (unsigned i = 0; i < TILE_MAX_DIM; i++)
        {
            ch[{(uint8_t)i, 0}].wall_west()  = wall1;
            ch[{(uint8_t)i, 1}].wall_north() = wall1;
            ch[{(uint8_t)i, 2}].wall_north() = wall2;
            ch[{(uint8_t)i, 2}].wall_west()  = wall2;
        }
    }

    for (int16_t i = -15; i <= 15; i++)
        for (int16_t j = -15; j <= 15; j++)
            w[{{i, j}, 0}].mark_modified();

    return w;
}

auto run(point from, point to, world& w, bool b, float len)
{
    constexpr float fuzz = iTILE_SIZE2.x();
    auto diag = rc::raycast_diag_s{};
    auto res = raycast_with_diag(diag, w, from, to, 0);
    if (res.success != b)
    {
        fm_error("success != %s", b ? "true" : "false");
        return false;
    }
    if (len != 0.f)
    {
        auto tmin = res.success ? diag.V.length() : diag.tmin;
        auto diff = Math::abs(tmin - len);
        if (diff > fuzz)
        {
            fm_error("|tmin=%f - len=%f| > %f",
                     (double)tmin, (double)len, (double)fuzz);
            return false;
        }
    }
    return true;
}

void Raycast(benchmark::State& state)
{
    auto w = make_world();

    const auto test = [&] {
      { constexpr auto from = point{{0, 0, 0}, {11,12}, {1,-32}};
        fm_assert(run(from, point{{  1,   3, 0}, { 0,  1}, {-21,  23}}, w, false,  2288));
        fm_assert(run(from, point{{  1,   3, 0}, { 8, 10}, {- 9, -13}}, w, true,   3075));
        fm_assert(run(from, point{{  0,   3, 0}, {14,  4}, {  3,  15}}, w, true,   2614));
        fm_assert(run(from, point{{  0,   1, 0}, { 8, 12}, {-27, -19}}, w, false,   752));
        fm_assert(run(from, point{{  2,  33, 0}, {15, 11}, {- 4,  29}}, w, true,  33809));
        fm_assert(run(from, point{{  0,   1, 0}, { 6, 13}, {- 3, -11}}, w, false,   913));
      }
      { fm_assert(run(      point{{  0,   0, 0}, { 1,  0}, {-17,  17}},
                            point{{  0, - 7, 0}, { 1, 15}, {-11,   5}}, w, true,   6220));
      }
    };

    for (int i = 0; i < 50; i++)
        test();
    for (auto _ : state)
        for (int i = 0; i < 1000; i++)
            test();
}

BENCHMARK(Raycast)->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
