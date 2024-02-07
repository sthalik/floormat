#include "impl.hpp"
#include "atlas-loader-storage.hpp"
#include "atlas-loader.inl"
#include "ground-traits.hpp"
#include "ground-cell.hpp"
#include "src/tile-constants.hpp"
#include "src/ground-atlas.hpp"
#include "compat/exception.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/ground-atlas.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>

namespace floormat::loader_detail {

template class atlas_loader<ground_atlas>;

} // namespace floormat::loader_detail

namespace floormat {

using loader_detail::atlas_loader_traits;
using ALT = atlas_loader_traits<ground_atlas>;

std::shared_ptr<ground_atlas>
loader_::get_ground_atlas(StringView name, Vector2ub size, pass_mode pass) noexcept(false)
{
    fm_assert(name != loader.INVALID);
    auto tex = texture(loader.GROUND_TILESET_PATH, name);
    auto info = ground_def{name, size, pass};
    auto atlas = std::make_shared<class ground_atlas>(info, tex);
    return atlas;
}

} // namespace floormat

namespace floormat::loader_detail {

atlas_loader<class ground_atlas>* loader_impl::make_ground_atlas_loader()
{
    return new atlas_loader<class ground_atlas>;
}

auto loader_impl::ground_atlas_list() noexcept(false) -> ArrayView<const ground_cell> { return _ground_loader->ensure_atlas_list(); }

const ground_cell& loader_impl::make_invalid_ground_atlas()
{
    return _ground_loader->get_invalid_atlas();
}

const std::shared_ptr<class ground_atlas>&
loader_impl::ground_atlas(StringView filename, loader_policy policy) noexcept(false)
{
    return _ground_loader->get_atlas(filename, policy);
}

} // namespace floormat::loader_detail
