#include "impl.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/scenery.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat::loader_detail {

anim_def loader_impl::deserialize_anim(StringView filename)
{
    return json_helper::from_json<anim_def>(filename);
}

} // namespace floormat::loader_detail

namespace floormat {

std::vector<std::shared_ptr<struct tile_atlas>> loader_::tile_atlases(StringView filename)
{
    return json_helper::from_json<std::vector<std::shared_ptr<struct tile_atlas>>>(Path::join(loader_::IMAGE_PATH, filename));
}

std::vector<Serialize::serialized_scenery> loader_::sceneries()
{
    return json_helper::from_json<std::vector<Serialize::serialized_scenery>>(Path::join(ANIM_PATH, "scenery.json"));
}

} // namespace floormat
