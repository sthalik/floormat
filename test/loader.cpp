#include "app.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "src/tile-atlas.hpp"
#include "loader/wall-info.hpp"

namespace floormat {

void test_app::test_loader()
{
    (void)loader.tile_atlases("wall.json", pass_mode::blocked);
    (void)loader.tile_atlases("floor.json", pass_mode::pass);
    fm_assert(loader.tile_atlas("texel")->pass_mode() == pass_mode::blocked);
    fm_assert(loader.tile_atlas("metal1")->pass_mode() == pass_mode::pass);
    loader.sceneries();
    for (StringView name : loader.anim_atlas_list())
        loader.anim_atlas(name);
    for (const auto& info : loader.wall_atlas_list())
        (void)loader.wall_atlas(info.name);
}

} // namespace floormat
