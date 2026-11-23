#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "src/world.hpp"
#include "src/anim-atlas.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/hole.hpp"
#include "loader/loader.hpp"

namespace floormat::Test {

namespace {

constexpr chunk_coords_ ch{0, 0, 0};

struct expected_queries
{
    object_type type;
    enum scenery_type scenery_type;
    bool is_virtual, is_dynamic, updates_passability, updates_walls;
};

template<typename T>
void check_queries(const T& e, const expected_queries& x)
{
    fm_assert(e.type() == x.type);
    fm_assert(e.type_of() == x.type);
    if constexpr (std::is_same_v<T, object>)
        fm_assert(object_proto(e).type == x.type);
    if (x.type == object_type::scenery)
        fm_assert(static_cast<const scenery&>(e).scenery_type() == x.scenery_type);
    fm_assert(e.is_virtual() == x.is_virtual);
    fm_assert(e.is_dynamic() == x.is_dynamic);
    fm_assert(e.updates_passability() == x.updates_passability);
    fm_assert(e.updates_walls() == x.updates_walls);
}

void test_queries_through_base_refs()
{
    auto w = world();
    const auto still_atlas = loader.anim_atlas("test-8x8", loader.ANIM_PATH);
    const auto animated_atlas = loader.make_placeholder_anim_atlas(loader.INVALID, {.fps = 30, .nframes = 2});
    fm_assert(still_atlas->info().fps == 0 && animated_atlas->info().fps > 0);

    auto still_critter = critter_proto{};
    still_critter.atlas = still_atlas;
    auto animated_generic = scenery_proto(loader.scenery("table1"));
    animated_generic.atlas = animated_atlas;
    auto still_door = scenery_proto(loader.scenery("door1"));
    still_door.atlas = still_atlas;
    still_door.frame = 0;

    const auto critterʹ = w.make_object<critter>(w.make_id(), {ch, {2, 2}}, critter_proto{});
    const auto still_critterʹ = w.make_object<critter>(w.make_id(), {ch, {2, 4}}, still_critter);
    const auto genericʹ = w.make_scenery(w.make_id(), {ch, {4, 2}}, scenery_proto(loader.scenery("table1")));
    const auto animated_genericʹ = w.make_scenery(w.make_id(), {ch, {4, 4}}, move(animated_generic));
    const auto doorʹ = w.make_scenery(w.make_id(), {ch, {6, 2}}, scenery_proto(loader.scenery("door1")));
    const auto still_doorʹ = w.make_scenery(w.make_id(), {ch, {6, 4}}, move(still_door));
    const auto lightʹ = w.make_object<light>(w.make_id(), {ch, {8, 2}}, light_proto{});
    const auto holeʹ = w.make_object<hole>(w.make_id(), {ch, {10, 2}}, hole_proto{});

    fm_assert(critterʹ->atlas->info().fps > 0 && genericʹ->atlas->info().fps == 0 && doorʹ->atlas->info().fps > 0);
    // A light's and a hole's is_dynamic() is true with a still atlas.
    fm_assert(lightʹ->atlas->info().fps == 0 && holeʹ->atlas->info().fps == 0);

    constexpr auto C = object_type::critter, S = object_type::scenery, L = object_type::light, H = object_type::hole;
    constexpr auto none = scenery_type::none, generic = scenery_type::generic, door = scenery_type::door;
    check_queries<object>(*critterʹ,            {C, none,    false, true,  false, false});
    check_queries<object>(*still_critterʹ,      {C, none,    false, false, false, false});
    check_queries<object>(*genericʹ,            {S, generic, false, false, false, false});
    check_queries<scenery>(*genericʹ,           {S, generic, false, false, false, false});
    check_queries<object>(*animated_genericʹ,   {S, generic, false, true,  false, false});
    check_queries<scenery>(*animated_genericʹ,  {S, generic, false, true,  false, false});
    check_queries<object>(*doorʹ,               {S, door,    false, true,  false, false});
    check_queries<scenery>(*doorʹ,              {S, door,    false, true,  false, false});
    check_queries<object>(*still_doorʹ,         {S, door,    false, false, false, false});
    check_queries<scenery>(*still_doorʹ,        {S, door,    false, false, false, false});
    check_queries<object>(*lightʹ,              {L, none,    true,  true,  false, false});
    check_queries<object>(*holeʹ,               {H, none,    true,  true,  true,  true});
}

} // namespace

void test_object_type_dispatch()
{
    test_queries_through_base_refs();
}

} // namespace floormat::Test
