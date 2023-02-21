#include "inspect.hpp"
#include "entity/accessor.hpp"
#include <imgui.h>

namespace floormat {

using namespace entities;

template<typename T>
static void do_inspect_field(const void* datum, const entities::erased_accessor& accessor)
{
    auto range = accessor.get_range(datum);
    auto enabled = accessor.is_enabled(datum);
    auto [min, max] = range.convert<T>();


}

template<> void inspect_field<int>(const void* datum, const entities::erased_accessor& accessor)
{
    do_inspect_field<int>(datum, accessor);
}

} // namespace floormat
