#include "impl.hpp"
#include "wall-cell.hpp"
#include "wall-traits.hpp"
#include "atlas-loader.inl"
#include "src/wall-atlas.hpp"
#include <cr/StringIterable.h>

namespace floormat::detail::loader {

template class atlas_loader<class wall_atlas>;

::floormat::bptr<class wall_atlas> loader_impl::get_wall_atlas(StringView name) noexcept(false)
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

::floormat::bptr<class wall_atlas> loader_impl::make_placeholder_wall_atlas(StringView name)
{
    return ::floormat::bptr<class wall_atlas>{InPlace, *invalid_wall_atlas().atlas, name};
}

const ::floormat::bptr<class wall_atlas>&
loader_impl::wall_atlas(StringView filename, loader_policy policy) noexcept(false)
{
    return _wall_loader->get_atlas(filename, policy);
}

} // namespace floormat::detail::loader
