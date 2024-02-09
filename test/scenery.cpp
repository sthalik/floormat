#include "app.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"

namespace floormat {

namespace {

void test_loading()
{
    fm_assert(!loader.sceneries().isEmpty());
}

} // namespace

void test_app::test_scenery()
{
    test_loading();
}

} // namespace floormat
