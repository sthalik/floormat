#include "vobj-editor.hpp"
#include "loader/vobj-info.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/light.hpp"
#include <array>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StringView.h>

#if defined __clang__ || defined __CLION_IDE__
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace floormat {

[[nodiscard]]
std::shared_ptr<entity> make_vobj(world& w, entity_type type, object_id id, global_coords pos);

StringView vobj_factory::name() const { return info().name; }
StringView vobj_factory::descr() const { return info().descr; }
std::shared_ptr<anim_atlas> vobj_factory::atlas() const { return info().atlas; }

struct light_factory final : vobj_factory
{
    entity_type type() const override { return entity_type::light; }

    const vobj_info& info() const override
    {
        constexpr auto NAME = "light"_s;
        static const vobj_info& ret = loader.vobj(NAME);
        fm_debug_assert(ret.name == NAME);
        return ret;
    }

    std::shared_ptr<entity> make(world& w, object_id id, global_coords pos) const override
    {
        auto ret = w.make_entity<light>(id, pos, light_proto{});
        return ret;
    }
};

template<typename T> struct factory_ { static constexpr const T value = {}; };

static consteval auto make_factory_array()
{
    const auto size = (1uz << entity_type_BITS)-1;
    std::array<const vobj_factory*, size> array = {};
    array[(unsigned)entity_type::light] = &factory_<light_factory>::value;
    return array;
}

static constexpr auto factory_array = make_factory_array();
const ArrayView<const vobj_factory* const> vobj_editor::_types = { factory_array.data(), factory_array.size() };

const vobj_factory* vobj_editor::get_factory(entity_type type)
{
    const auto idx = size_t(type);
    fm_debug_assert(idx < std::size(factory_array));
    const auto* ptr = factory_array[idx];
    if (!ptr)
    {
        fm_warn_once("invalid vobj type '%zu'", idx);
        return {};
    }
    else
        return ptr;
}

} // namespace floormat
