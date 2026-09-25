#include "app.hpp"
#include "src/chunk.hpp"
#include "src/object-storage.inl"
#include "src/world.hpp"
#include "src/object.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/hole.hpp"
#include "loader/loader.hpp"
#include "compat/borrowed-ptr.inl"
#include <utility>

namespace floormat {

namespace {

constexpr auto takes_critter = [](critter&) {};
constexpr auto takes_scenery = [](scenery&) {};
constexpr auto takes_const_object = [](const object&) {};
constexpr auto reads_light = [](auto& e) { (void)e.max_distance; };
constexpr auto mutates = [k = 0](light& e) mutable { e.frame = (uint16_t)k++; };
constexpr auto takes_list = []<typename T>(object_list<T>&) {};

static_assert(object_visitor<decltype(takes_critter), critter>);
static_assert(!object_visitor<decltype(takes_critter), critter, light>);
static_assert(!object_visitor<decltype(takes_critter), object>);
static_assert(object_visitor<decltype(takes_scenery), generic_scenery, door_scenery>);
static_assert(object_visitor<decltype(takes_const_object), object>);
static_assert(const_object_visitor<decltype(takes_const_object), object>);
static_assert(!const_object_visitor<decltype(takes_critter), critter>);
// hole has no max_distance, so this passes only if unselected classes aren't checked
static_assert(object_visitor<decltype(reads_light), light>);
static_assert(!object_visitor<decltype(mutates), light>);
static_assert(object_list_visitor<decltype(takes_list), object>);

template<typename T>
void check_list(chunk& c, object_list<T>& l)
{
    const auto size = l.size();
    for (auto i = 0u; i < size; i++)
    {
        const T& e = l[i];
        fm_assert(&e.chunk() == &c);
        fm_assert(e.type() == object_type_<T>::value);
        if constexpr (std::is_base_of_v<scenery, T>)
            fm_assert(e.scenery_type() == scenery_type_<T>::value);
        if (i > 0)
            fm_assert(l[i-1].id < e.id);
        fm_assert(e.index() == i);
        fm_assert(&*c.objects().ptr(e, i) == &e);
    }

    auto j = 0u;
    for (const T& e : std::as_const(l))
    {
        fm_assert(&e == &l[j]);
        j++;
    }
    fm_assert(j == size);
}

void check_invariants(chunk& c)
{
    c.objects().visit_lists([&]<typename T>(object_list<T>& l) { check_list(c, l); });

    const auto& s = std::as_const(c).objects();
    auto k = 0u;
    auto it = s.begin();
    s.visit([&](const object& e) {
        fm_assert(it != s.end());
        fm_assert(&*it == &e);
        fm_assert(&s[k] == &e);
        ++it;
        k++;
    });
    fm_assert(it == s.end());
    fm_assert(k == s.size());
}

bptr<critter> make_critter(world& w, object_id id, global_coords pos, bool sorted = true)
{
    critter_proto p;
    p.name = "critter"_s;
    if (sorted)
        return w.make_object<critter>(id, pos, p);
    else
        return w.make_object<critter, false>(id, pos, p);
}

void test_empty()
{
    world w;
    chunk_coords_ ch{0, 0, 0};
    chunk& c = w[ch];
    const chunk& cc = c;

    const auto& view = cc.objects();
    fm_assert(view.size() == 0);
    fm_assert(view.begin() == view.end());

    uint32_t n = 0;
    for ([[maybe_unused]] const object& o : view)
        ++n;
    fm_assert(n == 0);
}

void test_populated()
{
    world w;
    chunk_coords_ ch{0, 0, 0};
    chunk& c = Test::make_test_chunk(w, ch);
    c.sort_objects();

    auto& mut = c.objects();
    fm_assert(mut.size() > 0);

    const chunk& cc = c;
    const auto& view = cc.objects();
    fm_assert(view.size() == mut.size());

    uint32_t i = 0;
    for (const object& o : view)
    {
        fm_assert(&o == &*mut[i]);
        ++i;
    }
    fm_assert(i == view.size());

    for (uint32_t k = 0; k < view.size(); ++k)
        fm_assert(&view[k] == &*mut[k]);

    auto it = view.begin();
    auto end = view.end();
    uint32_t walked = 0;
    while (it != end)
    {
        ++it;
        ++walked;
    }
    fm_assert(walked == view.size());
}

void test_lists()
{
    world w;
    const auto ch = chunk_coords_{0, 0, 0}, ch2 = chunk_coords_{1, 0, 0};
    chunk& c = Test::make_test_chunk(w, ch);
    check_invariants(c);

    // used in reverse, so the unsorted appends leave each list out of order
    object_id ids[6];
    for (auto& id : ids)
        id = w.make_id();
    w.make_object<hole, false>(ids[5], {ch, {1, 1}}, hole_proto{});
    w.make_object<hole, false>(ids[4], {ch, {2, 1}}, hole_proto{});
    w.make_object<light, false>(ids[3], {ch, {3, 1}}, light_proto{});
    make_critter(w, ids[2], {ch, {5, 5}}, false);
    w.make_scenery<false>(ids[1], {ch, {6, 6}}, scenery_proto(loader.scenery("table1")));
    make_critter(w, ids[0], {ch, {7, 5}}, false);
    c.sort_objects();
    check_invariants(c);

    w.make_object<light>(w.make_id(), {ch, {9, 9}}, light_proto{});
    make_critter(w, w.make_id(), {ch2, {1, 1}});
    auto C = make_critter(w, w.make_id(), {ch, {10, 10}});
    check_invariants(c);

    auto& c2 = w[ch2];
    auto& critters = c.objects().list<critter>(), &critters2 = c2.objects().list<critter>();
    const auto n = critters.size();
    fm_assert(critters2.size() == 1);
    auto i = C->index();
    fm_assert(i == n - 1);
    C->teleport_to(i, global_coords{ch2, {3, 3}}, {}, rotation_COUNT);
    fm_assert(&C->chunk() == &c2);
    fm_assert(critters.size() == n - 1);
    fm_assert(critters2.size() == 2);
    fm_assert(i == 1);
    fm_assert(&critters2[1] == &*C);
    check_invariants(c);
    check_invariants(c2);

    const auto total = c.objects().size();
    auto& lights = c.objects().list<light>();
    fm_assert(lights.size() == 3);
    const auto* next = &lights[2];
    c.kill_object(lights[1], 1);
    fm_assert(lights.size() == 2);
    fm_assert(&lights[1] == next);
    fm_assert(c.objects().size() == total - 1);
    check_invariants(c);
}

} // namespace

void Test::test_chunk_iter()
{
    test_empty();
    test_populated();
    test_lists();
}

} // namespace floormat
