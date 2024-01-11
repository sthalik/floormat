#include "impl.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "src/ground-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "serialize/ground-atlas.hpp"
#include "serialize/scenery.hpp"
#include "loader/scenery.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Utility/Path.h>

namespace floormat::loader_detail {

anim_def loader_impl::deserialize_anim(StringView filename)
{
    return json_helper::from_json<anim_def>(filename);
}

void loader_impl::get_scenery_list()
{
    sceneries_array.clear();
    sceneries_array = json_helper::from_json<std::vector<serialized_scenery>>(Path::join(SCENERY_PATH, "scenery.json"));
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

ArrayView<const std::shared_ptr<class ground_atlas>> loader_impl::ground_atlases(StringView filename) noexcept(false)
{
    if (!ground_atlas_array.empty()) [[likely]]
        return ground_atlas_array;
    ground_atlas_array = json_helper::from_json<std::vector<std::shared_ptr<class ground_atlas>>>(
        Path::join(loader_::GROUND_TILESET_PATH, filename));
    fm_assert(!ground_atlas_array.empty());
    return ground_atlas_array;
}

} // namespace floormat::loader_detail
