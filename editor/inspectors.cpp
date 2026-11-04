#include "app.hpp"
#include <cr/GrowableArray.h>

namespace floormat {

namespace {
constexpr inline usize max_inspectors = 8;
} // namespace

void app::reserve_inspector_array()
{
    arrayReserve(inspectors, max_inspectors);
}

void app::add_inspector(popup_target p)
{
    if (inspectors.size() >= max_inspectors)
        arrayRemove(inspectors, 0, 1 + inspectors.size() - max_inspectors);
    arrayAppend(inspectors, p);
}

void app::erase_inspector(usize index, isize count)
{
    fm_assert(count >= 0);
    fm_assert(index < inspectors.size());
    fm_assert(index + (usize)count <= inspectors.size());
    arrayRemove(inspectors, index, (usize)count);
}

void app::kill_inspectors()
{
    arrayResize(inspectors, 0);
}

} // namespace floormat
