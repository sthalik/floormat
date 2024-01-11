#include "app.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "src/ground-atlas.hpp"
#include "loader/wall-info.hpp"

namespace floormat {

void test_app::test_loader()
{
    (void)loader.ground_atlases("floor.json");
    fm_assert(loader.ground_atlas("texel")->pass_mode() == pass_mode::blocked);
    fm_assert(loader.ground_atlas("metal1")->pass_mode() == pass_mode::pass);
    loader.sceneries();
    for (StringView name : loader.anim_atlas_list())
        loader.anim_atlas(name);
    (void)loader.wall_atlas_list();
#if 0
    for (const auto& info : loader.wall_atlas_list())
        (void)loader.wall_atlas(info.name);
#endif
}

} // namespace floormat
