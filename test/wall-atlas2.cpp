#include "app.hpp"
#include "compat/assert.hpp"
#include "src/tile-defs.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/wall-info.hpp"

namespace floormat {

void test_app::test_wall_atlas2()
{
    using enum Wall::Direction_;

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
    fm_assert(a.group(N, Wall::Group_::top)->is_defined);
    fm_assert(a.frames(N, Wall::Group_::wall)[0].size == Vector2ui(Vector2i{iTILE_SIZE.x(), iTILE_SIZE.z()}));
}

} // namespace floormat