#include "impl.hpp"
#include "compat/assert.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/scenery.hpp"
#include "loader/scenery.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat::loader_detail {

anim_def loader_impl::deserialize_anim(StringView filename)
{
    return json_helper::from_json<anim_def>(filename);
}

const std::vector<serialized_scenery>& loader_impl::sceneries()
{
    if (!sceneries_array.empty())
        return sceneries_array;

    sceneries_array = json_helper::from_json<std::vector<serialized_scenery>>(Path::join(SCENERY_PATH, "scenery.json"));
    sceneries_map.reserve(sceneries_array.size() * 2);
    for (const serialized_scenery& s : sceneries_array)
    {
        if (sceneries_map.contains(s.name))
            fm_abort("duplicate scenery name '%s'", s.name.data());
        sceneries_map[s.name] = &s;
    }
    return sceneries_array;
}

const scenery_proto& loader_impl::scenery(StringView name)
{
    if (sceneries_array.empty())
        (void)sceneries();
    auto it = sceneries_map.find(name);
    if (it == sceneries_map.end())
        fm_abort("no such scenery: '%s'", name.data());
    return it->second->proto;
}

} // namespace floormat::loader_detail

namespace floormat {

std::vector<std::shared_ptr<struct tile_atlas>> loader_::tile_atlases(StringView filename)
{
    return json_helper::from_json<std::vector<std::shared_ptr<struct tile_atlas>>>(Path::join(loader_::IMAGE_PATH, filename));
}

} // namespace floormat
