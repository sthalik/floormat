#include "app.hpp"
#include "compat/assert.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/wall-info.hpp"

namespace floormat {

void test_app::test_wall_atlas2()
{
    using enum Wall::Direction_;

    Debug{} << "test_wall2: start";
    static constexpr auto name = "concrete1"_s;
    auto& a = *loader.wall_atlas(name, false);
    fm_assert(a.name() == name);
    fm_assert(a.info().depth == 20);
    fm_assert(a.raw_frame_array().size() >= 3);
    fm_assert(!a.direction(W));
    fm_assert(a.direction(N));
    fm_assert(&a.calc_direction(W) == a.direction(N));
    fm_assert(&a.calc_direction(N) == a.direction(N));
    fm_assert(a.frames(N, Wall::Group_::wall).size() >= 3);
    Debug{} << "test_wall2: end";
}

} // namespace floormat
