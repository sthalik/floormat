#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "src/world.hpp"
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
}

void test_queries_through_base_refs()
{
    auto w = world();
    const auto critterʹ = w.make_object<critter>(w.make_id(), {ch, {2, 2}}, critter_proto{});
    const auto genericʹ = w.make_scenery(w.make_id(), {ch, {4, 2}}, scenery_proto(loader.scenery("table1")));
    const auto doorʹ = w.make_scenery(w.make_id(), {ch, {6, 2}}, scenery_proto(loader.scenery("door1")));
    const auto lightʹ = w.make_object<light>(w.make_id(), {ch, {8, 2}}, light_proto{});
    const auto holeʹ = w.make_object<hole>(w.make_id(), {ch, {10, 2}}, hole_proto{});

    check_queries<object>(*critterʹ, {object_type::critter, scenery_type::none});
    check_queries<object>(*genericʹ, {object_type::scenery, scenery_type::generic});
    check_queries<scenery>(*genericʹ, {object_type::scenery, scenery_type::generic});
    check_queries<object>(*doorʹ, {object_type::scenery, scenery_type::door});
    check_queries<scenery>(*doorʹ, {object_type::scenery, scenery_type::door});
    check_queries<object>(*lightʹ, {object_type::light, scenery_type::none});
    check_queries<object>(*holeʹ, {object_type::hole, scenery_type::none});
}

} // namespace

void test_object_type_dispatch()
{
    test_queries_through_base_refs();
}

} // namespace floormat::Test
