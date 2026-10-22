#include "test/app.hpp"
#include "compat/exception.hpp"
#include "src/scenery-proto.hpp"
#include "serialize/scenery.hpp"
#include "serialize/magnum-vector.hpp"
#include "loader/loader.hpp"
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

} // namespace

void test_scenery()
{
    test_scenery_bad_type();
    test_scenery_odd_bbox();
}

} // namespace floormat::Test
