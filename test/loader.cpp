#include "app.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "src/ground-atlas.hpp"
#include "loader/wall-info.hpp"

namespace floormat {

void test_app::test_loader()
{
    for (const auto& x : loader.ground_atlas_list())
        (void)loader.ground_atlas(x.name);
    fm_assert(loader.ground_atlas("texel")->pass_mode() == pass_mode::blocked);
    fm_assert(loader.ground_atlas("metal1")->pass_mode() == pass_mode::pass);
    loader.sceneries();
    for (StringView name : loader.anim_atlas_list())
        loader.anim_atlas(name);

    {   auto walls = loader.wall_atlas_list();
        fm_assert(!walls.isEmpty());
        fm_assert(loader.wall_atlas("test1"_s));
        fm_assert(loader.wall_atlas(loader.INVALID, true));
        fm_assert(loader.wall_atlas("test1"_s) == loader.wall_atlas("test1"_s));
        fm_assert(loader.wall_atlas("test1"_s) != loader.wall_atlas(loader.INVALID, true));
    }
    for (const auto& info : loader.wall_atlas_list())
        fm_assert(loader.wall_atlas(info.name));
}

} // namespace floormat
