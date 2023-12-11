#include "app.hpp"
#include "compat/assert.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/wall-info.hpp"

namespace floormat {

void test_app::test_wall_atlas2()
{
    Debug{} << "test_wall2: start";
    static constexpr auto name = "concrete1"_s;
    auto& a = *loader.wall_atlas(name, false);
    fm_assert(a.name() == name);
    fm_assert(a.info().depth == 20);
    Debug{} << "test_wall2: end";
}

} // namespace floormat
