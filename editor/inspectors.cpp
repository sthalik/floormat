#include "app.hpp"
#include <Corrade/Containers/GrowableArray.h>

namespace floormat {

namespace {
constexpr inline size_t max_inspectors = 2;
} // namespace

void app::reserve_inspector_array()
{
    arrayReserve(inspectors, max_inspectors);
}

void app::add_inspector(popup_target p)
{
    if (inspectors.size() >= max_inspectors)
        arrayRemove(inspectors, 1 + inspectors.size() - max_inspectors);

    arrayAppend(inspectors, p);
}

void app::erase_inspector(size_t index, ptrdiff_t count)
{
    fm_assert(count >= 0);
    fm_assert(index < inspectors.size());
    fm_assert(index + (size_t)count <= inspectors.size());
    arrayRemove(inspectors, index, (size_t)count);
}

void app::kill_inspectors()
{
    arrayResize(inspectors, 0);
}

} // namespace floormat
