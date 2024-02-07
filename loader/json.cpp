#include "impl.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "src/ground-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "serialize/scenery.hpp"
#include "loader/scenery.hpp"
#include "loader/anim-cell.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Utility/Path.h>

namespace floormat {

anim_def loader_::deserialize_anim_def(StringView filename) noexcept(false)
{
    return json_helper::from_json<anim_def>(filename);
}

} // namespace floormat

namespace floormat::loader_detail {

void loader_impl::get_scenery_list()
{
    sceneries_array.clear();
    sceneries_array = json_helper::from_json<std::vector<serialized_scenery>>(Path::join(SCENERY_PATH, "scenery.json"));

    if constexpr(true) // todo!
    {
        auto proto = scenery_proto{};
        proto.atlas = make_invalid_anim_atlas().atlas;
        proto.bbox_size = Vector2ub{20};
        proto.subtype = generic_scenery_proto{false, true};
        sceneries_array.push_back({ .name = INVALID, .proto = proto });
    }

    sceneries_map.clear();
    sceneries_map.reserve(sceneries_array.size() * 2);

    for (const serialized_scenery& s : sceneries_array)
    {
        if (sceneries_map.contains(s.name))
            fm_abort("duplicate scenery name '%s'", s.name.data());
        sceneries_map[s.name] = &s;
    }

    fm_assert(!sceneries_map.empty());
}

ArrayView<const serialized_scenery> loader_impl::sceneries()
{
    if (sceneries_array.empty()) [[likely]]
        get_scenery_list();
    fm_assert(!sceneries_array.empty());
    return sceneries_array;
}

const scenery_proto& loader_impl::scenery(StringView name) noexcept(false)
{
    fm_soft_assert(check_atlas_name(name));
    if (sceneries_array.empty())
        get_scenery_list();
    fm_assert(!sceneries_array.empty());
    auto it = sceneries_map.find(name);
    if (it == sceneries_map.end())
        fm_throw("no such scenery: '{}'"_cf, name);
    return it->second->proto;
}

} // namespace floormat::loader_detail
