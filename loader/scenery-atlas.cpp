#include "impl.hpp"
#include "atlas-loader.inl"
#include "scenery-traits.hpp"
#include "scenery-cell.hpp"
#include "src/anim.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "serialize/scenery.hpp"
#include "serialize/json-wrapper.hpp"

namespace floormat {

anim_def loader_::deserialize_anim_def(StringView filename) noexcept(false)
{
    return json_helper::from_json<anim_def>(filename);
}

} // namespace floormat

namespace floormat::loader_detail {

template class atlas_loader<struct scenery_proto>;

atlas_loader<struct scenery_proto>* loader_impl::make_scenery_atlas_loader()
{
    return new atlas_loader<struct scenery_proto>;
}

ArrayView<const scenery_cell> loader_impl::scenery_list()
{
    return _scenery_loader->atlas_list();
}

const struct scenery_proto& loader_impl::scenery(StringView name, loader_policy policy)
{
    return *_scenery_loader->get_atlas(name, policy);
}

const scenery_cell& loader_impl::invalid_scenery_atlas()
{
    return _scenery_loader->get_invalid_atlas();
}

struct scenery_proto loader_impl::get_scenery(StringView filename, const scenery_cell& c) noexcept(false)
{
    return *_scenery_loader->make_atlas(filename, c);
}

} // namespace floormat::loader_detail
