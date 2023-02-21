#include "inspect.hpp"
#include "compat/assert.hpp"
#include "entity/accessor.hpp"
#include "imgui-raii.hpp"

namespace floormat {

using namespace imgui;
using namespace entities;

template<typename T>
static void do_inspect_field(const void* datum, const erased_accessor& accessor)
{
    fm_assert(accessor.check_field_name<T>());
    raii_wrapper disabler;

    switch (accessor.is_enabled(datum))
    {
    using enum field_status;
    case hidden: return;
    case readonly: disabler = begin_disabled(); break;
    case enabled: break;
    }

    auto [min, max] = accessor.get_range(datum).convert<T>();
}

template<> void inspect_field<int>(const void* datum, const erased_accessor& accessor)
{
    do_inspect_field<int>(datum, accessor);
}

} // namespace floormat
