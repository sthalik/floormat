#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/nanosecond.inl"
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

struct door_state
{
    u16 frame;
    u32 delta;
    pass_mode pass;
    bool closing, active;

    bool operator==(const door_state&) const = default;
};

door_state state_of(const door_scenery& d)
{
    return { d.frame, d.delta, d.pass, d.closing, d.active };
}

void test_activate_through_base_ref()
{
    auto w = world();
    auto door_proto = scenery_proto(loader.scenery("door1"));
    const auto* door_subtype = swl::get_if<door_scenery_proto>(&door_proto.subtype);
    fm_assert(door_subtype && door_subtype->interactive);
    const auto doorʹ = w.make_scenery(w.make_id(), {ch, {4, 4}}, scenery_proto(door_proto));
    const auto door2ʹ = w.make_scenery(w.make_id(), {ch, {8, 4}}, scenery_proto(door_proto));
    auto& door = static_cast<door_scenery&>(*doorʹ);
    auto& door2 = static_cast<door_scenery&>(*door2ʹ);
    object& o = door;
    const auto frame0 = door.frame;
    fm_assert(!door.active && state_of(door) == state_of(door2));

    const bool can_activate_door = o.can_activate(o.index());
    fm_assert(can_activate_door && can_activate_door == door2.can_activate(door2.index()));
    const bool is_door_activated = o.activate(o.index());
    fm_assert(is_door_activated && is_door_activated == door2.activate(door2.index()));
    fm_assert(door.active && door.frame != frame0 && state_of(door) == state_of(door2));
    fm_assert(!o.activate(o.index()) && !door2.activate(door2.index()));
    fm_assert(state_of(door) == state_of(door2));

    u32 num_updates = 0;
    while (door.active)
    {
        auto i = o.index(), i2 = door2.index();
        o.update(doorʹ, i, Second/60);
        door2.update(door2ʹ, i2, Second/60);
        fm_assert(state_of(door) == state_of(door2));
        num_updates++;
        fm_assert(num_updates < 60*60);
    }
    const auto last_frame = (u16)(door.atlas->info().nframes - 1);
    fm_assert(num_updates > 1 && door.frame == (frame0 == 0 ? last_frame : 0));

    auto table = scenery_proto(loader.scenery("table1"));
    auto* table_subtype = swl::get_if<generic_scenery_proto>(&table.subtype);
    fm_assert(table_subtype && !table_subtype->interactive);
    const auto genericʹ = w.make_scenery(w.make_id(), {ch, {2, 8}}, scenery_proto(table));
    table_subtype->interactive = true;
    const auto interactive_genericʹ = w.make_scenery(w.make_id(), {ch, {4, 8}}, scenery_proto(table));
    const auto critterʹ = w.make_object<critter>(w.make_id(), {ch, {6, 8}}, critter_proto{});
    const auto lightʹ = w.make_object<light>(w.make_id(), {ch, {8, 8}}, light_proto{});
    const auto holeʹ = w.make_object<hole>(w.make_id(), {ch, {10, 8}}, hole_proto{});

    const struct { object& e; bool can_activate; } others[] = {
        { *genericʹ, false }, { *interactive_genericʹ, true },
        { *critterʹ, false }, { *lightʹ, false }, { *holeʹ, false },
    };
    for (const auto& [e, can_activate] : others)
    {
        fm_assert(e.can_activate(e.index()) == can_activate);
        fm_assert(!e.activate(e.index()));
    }
    const auto& g = static_cast<const generic_scenery&>(*interactive_genericʹ);
    fm_assert(!g.active);
}

void test_script_lifecycle_through_base_ref()
{
    auto w = world();
    const auto Cʹ = w.make_object<critter>(w.make_id(), {ch, {2, 2}}, critter_proto{});
    const auto genericʹ = w.make_scenery(w.make_id(), {ch, {4, 2}}, scenery_proto(loader.scenery("table1")));
    const auto doorʹ = w.make_scenery(w.make_id(), {ch, {6, 2}}, scenery_proto(loader.scenery("door1")));
    const auto lightʹ = w.make_object<light>(w.make_id(), {ch, {8, 2}}, light_proto{});
    const auto holeʹ = w.make_object<hole>(w.make_id(), {ch, {10, 2}}, hole_proto{});
    fm_assert(!w.script_status().initialized);

    const bptr<object> oʹ = Cʹ;
    auto& C = *Cʹ;
    object& o = C;
    fm_assert(C.script.state() == script_lifecycle::no_init && !C.script);
    o.init_script(oʹ);
    fm_assert(C.script.state() == script_lifecycle::created && C.script);
    o.destroy_script_pre(oʹ, script_destroy_reason::quit);
    fm_assert(C.script.state() == script_lifecycle::destroying && C.script);
    o.destroy_script_post();
    fm_assert(C.script.state() == script_lifecycle::torn_down && !C.script);

    const auto protos_equal = [&](const scenery_proto& generic, const scenery_proto& door, const light_proto& l,
                                  const hole_proto& h) {
        return scenery_proto(*genericʹ) == generic && scenery_proto(*doorʹ) == door &&
               light_proto(*lightʹ) == l && hole_proto(*holeʹ) == h;
    };
    const auto generic0 = scenery_proto(*genericʹ), door0 = scenery_proto(*doorʹ);
    const auto light0 = light_proto(*lightʹ);
    const auto hole0 = hole_proto(*holeʹ);
    const auto pass_gen = w.pass_gen();
    for (const bptr<object>& eʹ : { bptr<object>(genericʹ), bptr<object>(doorʹ), bptr<object>(lightʹ),
                                    bptr<object>(holeʹ) })
    {
        eʹ->init_script(eʹ);
        eʹ->destroy_script_pre(eʹ, script_destroy_reason::quit);
        eʹ->destroy_script_post();
        fm_assert(protos_equal(generic0, door0, light0, hole0));
        fm_assert_equal(pass_gen, w.pass_gen());
    }
}

struct chunk_marks
{
    u64 pass_gen, neighbor_pass_gen, objects_gen;
    bool is_passability_modified, are_walls_modified;

    bool operator==(const chunk_marks&) const = default;
};

chunk_marks marks_of(const chunk& c)
{
    return { c.pass_gen(), c.neighbor_pass_gen(), c.objects_gen(), c.is_passability_modified(), c.are_walls_modified() };
}

void test_mark_modified_through_base_ref()
{
    auto w = world();
    for (i16 j = -1; j <= 1; j++)
        for (i16 i = -1; i <= 1; i++)
            (void)w[chunk_coords_{i, j, 0}];
    auto& c = w[ch];
    const auto neighbors = w.neighbors(ch);
    for (const auto* n : neighbors)
        fm_assert(n);

    const auto holeʹ = w.make_object<hole>(w.make_id(), {ch, {8, 8}}, hole_proto{});
    const auto critterʹ = w.make_object<critter>(w.make_id(), {ch, {2, 2}}, critter_proto{});
    const auto genericʹ = w.make_scenery(w.make_id(), {ch, {4, 2}}, scenery_proto(loader.scenery("table1")));
    const auto doorʹ = w.make_scenery(w.make_id(), {ch, {6, 2}}, scenery_proto(loader.scenery("door1")));
    const auto lightʹ = w.make_object<light>(w.make_id(), {ch, {10, 2}}, light_proto{});

    // Element 0 is the hole's chunk, the rest are its neighbors.
    const auto marks = [&] {
        std::array<chunk_marks, 9> ret;
        ret[0] = marks_of(c);
        for (auto k = 0u; k < 8; k++)
            ret[k+1] = marks_of(*neighbors[k]);
        return ret;
    };

    w.ensure_passability();
    const auto before = marks();
    for (const auto& m : before)
        fm_assert(!m.is_passability_modified);

    for (object* e : std::array<object*, 4>{ &*critterʹ, &*genericʹ, &*doorʹ, &*lightʹ })
    {
        e->mark_modified_with_neighbors();
        fm_assert(marks() == before);
    }

    static_cast<object&>(*holeʹ).mark_modified_with_neighbors();
    const auto after = marks();
    fm_assert_not_equal(before[0].pass_gen, after[0].pass_gen);
    fm_assert_equal(before[0].neighbor_pass_gen, after[0].neighbor_pass_gen);
    fm_assert(after[0].is_passability_modified && after[0].are_walls_modified);
    for (auto k = 1u; k < 9; k++)
    {
        fm_assert_equal(before[k].pass_gen, after[k].pass_gen);
        fm_assert_not_equal(before[k].neighbor_pass_gen, after[k].neighbor_pass_gen);
        fm_assert(after[k].is_passability_modified && after[k].are_walls_modified);
    }
    for (auto k = 0u; k < 9; k++)
        fm_assert_equal(before[k].objects_gen, after[k].objects_gen);
}

void test_scenery_proto_through_base_ref()
{
    auto w = world();
    auto table = scenery_proto(loader.scenery("table1"));
    auto* table_subtype = swl::get_if<generic_scenery_proto>(&table.subtype);
    fm_assert(table_subtype);
    table_subtype->interactive = true;
    const auto genericʹ = w.make_scenery(w.make_id(), {ch, {2, 2}}, scenery_proto(table));
    const auto doorʹ = w.make_scenery(w.make_id(), {ch, {4, 2}}, scenery_proto(loader.scenery("door1")));
    auto& door = static_cast<door_scenery&>(*doorʹ);
    fm_assert(door.activate(door.index()) && door.active);

    const auto& generic = static_cast<const generic_scenery&>(*genericʹ);
    const auto generic_proto = scenery_proto(generic), door_proto = scenery_proto(door);
    const auto* generic_subtype = swl::get_if<generic_scenery_proto>(&generic_proto.subtype);
    const auto* door_subtype = swl::get_if<door_scenery_proto>(&door_proto.subtype);
    fm_assert(generic_subtype && generic_subtype->interactive);
    fm_assert(door_subtype && door_subtype->active && door_subtype->interactive);

    const scenery& generic_as_scenery = generic, & door_as_scenery = door;
    fm_assert(scenery_proto(generic_as_scenery) == generic_proto);
    fm_assert(scenery_proto(door_as_scenery) == door_proto);
}

} // namespace

void test_object_type_dispatch()
{
    test_queries_through_base_refs();
    test_activate_through_base_ref();
    test_script_lifecycle_through_base_ref();
    test_mark_modified_through_base_ref();
    test_scenery_proto_through_base_ref();
}

} // namespace floormat::Test
