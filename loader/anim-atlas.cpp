#include "impl.hpp"
#include "atlas-loader.inl"
#include "anim-cell.hpp"
#include "anim-traits.hpp"
#include "compat/exception.hpp"

namespace floormat::loader_detail {

template class atlas_loader<anim_atlas>;

std::shared_ptr<class anim_atlas> loader_impl::get_anim_atlas(StringView path) noexcept(false)
{
    return _anim_loader->make_atlas(path, {});
}

atlas_loader<class anim_atlas>* loader_impl::make_anim_atlas_loader()
{
    return new atlas_loader<class anim_atlas>;
}

ArrayView<const anim_cell> loader_impl::anim_atlas_list()
{
    return _anim_loader->atlas_list();
}

std::shared_ptr<anim_atlas> loader_impl::anim_atlas(StringView name, StringView dir, loader_policy p) noexcept(false)
{
    char buf[fm_FILENAME_MAX];
    auto path = make_atlas_path(buf, dir, name);
    return _anim_loader->get_atlas(path, p);
}

const anim_cell& loader_impl::invalid_anim_atlas()
{
    return _anim_loader->get_invalid_atlas();
}

} // namespace floormat::loader_detail
