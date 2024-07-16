#include "app.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
#include "loader/anim-cell.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "src/scenery-proto.hpp"
#include "compat/borrowed-ptr.inl"

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
};

constexpr const char* sceneries[] = {
    "bench1",
    "shelf6",
    "door1",
    "control panel (wall) 1",
    "table0",
};

} // namespace

void Test::test_loader()
{
    fm_assert(loader.ground_atlas("texel")->pass_mode() == pass_mode::blocked);
}

void Test::test_loader2()
{
    constexpr auto nonexistent = "__/nonexistent/__"_s;

    fm_assert(loader.ground_atlas(nonexistent, loader_policy::ignore) == loader.invalid_ground_atlas().atlas);
    fm_assert(loader.wall_atlas(nonexistent, loader_policy::ignore) == loader.invalid_wall_atlas().atlas);

    fm_assert(loader.get_ground_atlas("texel"_s, {2, 2}, pass_mode::blocked)->name() == "texel"_s);

    fm_assert(loader.ground_atlas("metal1")->pass_mode() == pass_mode::pass);

    fm_assert(loader.invalid_ground_atlas().atlas);
    fm_assert(&loader.invalid_ground_atlas().atlas == &loader.invalid_ground_atlas().atlas);
    fm_assert(loader.invalid_ground_atlas().name == loader.INVALID);

    fm_assert(loader.invalid_wall_atlas().atlas);
    fm_assert(&loader.invalid_wall_atlas().atlas == &loader.invalid_wall_atlas().atlas);
    fm_assert(loader.invalid_wall_atlas().name == loader.INVALID);

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
    fm_assert(!loader.scenery_list().isEmpty());
    for (const auto& name : sceneries)
    {
        const auto& S = loader.scenery(name);
        fm_assert(S.atlas);
    }
}

void Test::test_loader3()
{
    for (const auto& x : loader.ground_atlas_list())
    {
        fm_assert(x.name);
        if (x.name == loader.INVALID)
            continue;
        if (!x.atlas)
        {
            auto atlas = loader.ground_atlas(x.name, loader_policy::error);
            fm_assert(atlas->name() == x.name);
            fm_assert(atlas->texture().id());
            fm_assert(!atlas->pixel_size().isZero());
            fm_assert(Vector2ui{atlas->num_tiles2()}.product());
        }
    }
    for (const auto& x : loader.wall_atlas_list())
    {
        fm_assert(x.name);
        if (x.name == loader.INVALID)
            continue;
        if (!x.atlas)
        {
            auto atlas = loader.wall_atlas(x.name, loader_policy::error);
            fm_assert(atlas->name() == x.name);
            fm_assert(atlas->texture().id());
            fm_assert(!atlas->raw_frame_array().isEmpty());
            fm_assert(atlas->calc_direction(Wall::Direction_::N).wall.count);
        }
    }
    for (const auto& x : loader.anim_atlas_list())
    {
        fm_assert(x.name);
        if (x.name == loader.INVALID)
            continue;
        auto atlas_ = loader.anim_atlas(x.name, {}, loader_policy::error);
        fm_assert(atlas_);
        auto& atlas = *atlas_;
        fm_assert(atlas.name() == x.name);
        fm_assert(atlas.texture().id());
        fm_assert(atlas.info().nframes > 0);
    }
    // todo scenery_cell
}

} // namespace floormat
