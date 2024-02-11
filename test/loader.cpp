#include "app.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include <mg/Texture.h>

namespace floormat {

namespace {

// copied from bench/loader.cpp

constexpr struct {
    const char* name;
    Vector2ub size;
    pass_mode pass = pass_mode::pass;
} ground_atlases[] = {
    { "floor-tiles", {44,4} },
    { "tiles", {8, 5} },
    { "texel", {2, 2}, pass_mode::blocked },
    { "metal1", {2,2} },
};

constexpr const char* wall_atlases[] = {
    "concrete1", "empty", "test1",
};

constexpr const char* anim_atlases[] = {
    "anim/npc-walk",
    "anim/test-8x8",
    "scenery/door-close",
    "scenery/control-panel",
    "scenery/table",
};

} // namespace

void test_app::test_loader()
{
    fm_assert(loader.make_invalid_ground_atlas().atlas);
    fm_assert(&loader.make_invalid_ground_atlas().atlas == &loader.make_invalid_ground_atlas().atlas);
    fm_assert(loader.make_invalid_ground_atlas().name == loader.INVALID);

    fm_assert(loader.make_invalid_wall_atlas().atlas);
    fm_assert(&loader.make_invalid_wall_atlas().atlas == &loader.make_invalid_wall_atlas().atlas);
    fm_assert(loader.make_invalid_wall_atlas().name == loader.INVALID);

    for (const auto& str : anim_atlases)
        (void)loader.anim_atlas(str, {});
    for (const auto& x : ground_atlases)
    {
        auto& A = *loader.ground_atlas(x.name);
        fm_assert(A.num_tiles2() == x.size);
        fm_assert(A.pass_mode() == x.pass);
        fm_assert(A.texture().id());
    }
    for (const auto& name : wall_atlases)
    {
        auto& A = *loader.wall_atlas(name);
        fm_assert(!A.raw_frame_array().isEmpty());
        fm_assert(A.texture().id());
    }

    for (const auto& x : loader.ground_atlas_list())
    {
        if (x.name != loader.INVALID)
        {
            (void)loader.ground_atlas(x.name);
            fm_assert(x.atlas);
            fm_assert(x.atlas == loader.ground_atlas(x.name));
        }
        else
        {
            fm_assert(x.atlas);
            fm_assert(x.atlas == loader.make_invalid_ground_atlas().atlas);
        }
    }
    fm_assert(loader.ground_atlas("texel")->pass_mode() == pass_mode::blocked);
    fm_assert(loader.ground_atlas("metal1")->pass_mode() == pass_mode::pass);
    loader.sceneries();
}

} // namespace floormat
