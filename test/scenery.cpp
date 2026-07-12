#include "test/app.hpp"
#include "compat/exception.hpp"
#include "src/scenery-proto.hpp"
#include "serialize/scenery.hpp"
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

} // namespace

void test_scenery()
{
    test_scenery_bad_type();
}

} // namespace floormat::Test
