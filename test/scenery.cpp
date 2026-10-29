#include "test/app.hpp"
#include "compat/exception.hpp"
#include "src/scenery-proto.hpp"
#include "src/scenery.hpp"
#include "src/world.hpp"
#include "src/nanosecond.hpp"
#include "compat/borrowed-ptr.inl"
#include "serialize/scenery.hpp"
#include "serialize/magnum-vector.hpp"
#include "loader/loader.hpp"
#include "loader/anim-cell.hpp"
#include <nlohmann/json.hpp>

namespace floormat::Test {

namespace {

using nlohmann::json;

void test_scenery_bad_type()
{
    json j;
    j["atlas-name"] = "table"; // from_json prepends SCENERY_PATH to resolve this
    (void)j.get<scenery_proto>(); // baseline: a valid generic scenery deserializes
    // "none" parses (it's a live entry in scenery_type_map) rather than throwing at from_string
    j["type"] = "none";
    bool caught = false;
    try { auto f = j.get<scenery_proto>(); (void)f; }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(caught);
}

void test_scenery_odd_bbox()
{
    json j;
    j["atlas-name"] = "table";
    j["bbox-offset"] = Vector2i{-3, 5};
    j["bbox-size"] = Vector2ui{63, 25};
    const auto f = j.get<scenery_proto>();
    fm_assert(f.bbox_size == Vector2ub(63, 25));
    fm_assert(f.bbox_offset == Vector2b(-3, 5));
    const json j2 = f;
    fm_assert(j2.at("bbox-size").get<Vector2ui>() == Vector2ui(63, 25));
}

void test_scenery_atlas_name_round_trip()
{
    for (const char* name : { "table", "furniture/bench1" })
    {
        json j;
        j["atlas-name"] = name;
        const auto f = j.get<scenery_proto>();
        const json j2 = f;
        fm_assert(j2.at("atlas-name").get<std::string>() == name);
        fm_assert(j2.get<scenery_proto>().atlas == f.atlas);
    }
}

void test_scenery_door_fps_zero()
{
    auto w = world();
    auto proto = loader.scenery("door1");
    proto.atlas = loader.anim_atlas("test-8x8", loader.ANIM_PATH);
    proto.frame = 0;
    const auto eʹ = w.make_scenery(w.make_id(), {{0, 0, 0}, {8, 8}}, move(proto));
    fm_assert(eʹ->scenery_type() == scenery_type::door);
    auto& e = static_cast<door_scenery&>(*eʹ);
    e.active = true;
    auto index = e.index();
    e.update(eʹ, index, Second);
    fm_assert(e.frame == 0 && e.active);

    e.active = false;
    fm_assert(e.activate(index));
    fm_assert(e.frame == 0 && e.active);
}

} // namespace

void test_scenery()
{
    test_scenery_bad_type();
    test_scenery_odd_bbox();
    test_scenery_atlas_name_round_trip();
    test_scenery_door_fps_zero();
}

} // namespace floormat::Test
