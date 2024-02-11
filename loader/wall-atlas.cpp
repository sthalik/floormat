#include "loader/impl.hpp"
#include "src/wall-atlas.hpp"
#include "loader/wall-cell.hpp"
#include "loader/wall-traits.hpp"
#include "loader/atlas-loader.inl"
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringIterable.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>

namespace floormat::loader_detail {

template class atlas_loader<class wall_atlas>;

std::shared_ptr<class wall_atlas>
loader_impl::get_wall_atlas(StringView name) noexcept(false)
{
    return _wall_loader->make_atlas(name, {});
}

atlas_loader<class wall_atlas>* loader_impl::make_wall_atlas_loader()
{
    return new atlas_loader<class wall_atlas>;
}

ArrayView<const wall_cell> loader_impl::wall_atlas_list() noexcept(false)
{
    return _wall_loader->atlas_list();
}

const wall_cell& loader_impl::invalid_wall_atlas()
{
    return _wall_loader->get_invalid_atlas();
}

const std::shared_ptr<class wall_atlas>&
loader_impl::wall_atlas(StringView filename, loader_policy policy) noexcept(false)
{
    return _wall_loader->get_atlas(filename, policy);
}

} // namespace floormat::loader_detail
