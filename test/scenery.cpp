#include "app.hpp"
#include "loader/loader.hpp"
#include "loader/scenery.hpp"

namespace floormat {

namespace {

void test_loading()
{
    fm_assert(!loader.sceneries().isEmpty());

    for (const auto& [name, descr, proto] : loader.sceneries())
        fm_assert(proto.sc_type != scenery_type::none);
}

} // namespace

void test_app::test_scenery()
{
    test_loading();
}

} // namespace floormat
