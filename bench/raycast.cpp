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

    w[global_coords{{0, 1, 0}, { 8,  8}}].t.wall_north() = wall1;
    w[global_coords{{0, 1, 0}, {10,  8}}].t.wall_north() = wall1;
    w[global_coords{{0, 1, 0}, {12,  8}}].t.wall_west()  = wall1;

    w[global_coords{{0, 2, 0}, { 9,  0}}].t.wall_north() = wall1;
    w[global_coords{{0, 2, 0}, {10,  0}}].t.wall_north() = wall1;

    for (int16_t i = -10; i <= 10; i++)
        for (int16_t j = -10; j <= 10; j++)
            w[{{i, j}, 0}].mark_modified();

    return w;
}

[[maybe_unused]] void Raycast(benchmark::State& state)
{
    constexpr auto run1 = [](world& w, point to, bool b, float len = 0)
    {
        constexpr auto from = point{{0, 0, 0}, {11,12}, {1,-32}};
        constexpr float fuzz = iTILE_SIZE2.x();
        auto diag = rc::raycast_diag_s{};
        auto res = raycast_with_diag(diag, w, from, to, 0);
        if (res.success != b)
        {
            fm_error("success != %s", b ? "true" : "false");
            return false;
        }
        if (len > 0)
        {
            auto tmin = res.success ? diag.V.length() : diag.tmin;
            fm_assert(len > 1e-6f);
            auto diff = Math::abs(tmin - len);
            if (diff > fuzz)
            {
                fm_error("|tmin=%f - len=%f| > %f",
                         (double)tmin, (double)len, (double)fuzz);
                return false;
            }
        }
        return true;
    };

    auto w = make_world();

    const auto run = [&] {
        fm_assert(run1(w, point{{ 1,  3, 0}, { 0,  1}, {-21,  23}}, false,  2288));
        fm_assert(run1(w, point{{ 1,  3, 0}, { 8, 10}, {- 9, -13}}, true,   3075));
        fm_assert(run1(w, point{{ 0,  3, 0}, {14,  4}, {  3,  15}}, true,   2614));
        fm_assert(run1(w, point{{ 0,  1, 0}, { 8, 12}, {-27, -19}}, false,   752));
        //fm_assert(run1(w, point{{ 0,  1, 0}, { 7, 11}, {- 8, -21}}, false,   908));
        fm_assert(run1(w, point{{ 2, 33, 0}, {15, 11}, {- 4,  29}}, true,  33809));
    };

    run();
    for (auto _ : state)
        run();
}

BENCHMARK(Raycast)->Unit(benchmark::kMicrosecond);

} // namespace

} // namespace floormat
