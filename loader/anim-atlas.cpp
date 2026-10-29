#include "impl.hpp"
#include "atlas-loader.inl"
#include "anim-cell.hpp"
#include "anim-traits.hpp"
#include "compat/borrowed-ptr.hpp"

namespace floormat::loader_detail {

template class atlas_loader<anim_atlas>;

bptr<class anim_atlas> loader_impl::get_anim_atlas(StringView path) noexcept(false)
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

bptr<anim_atlas> loader_impl::anim_atlas(StringView name, StringView dir, loader_policy p) noexcept(false)
{
    char buf[fm_FILENAME_MAX];
    auto path = make_atlas_path(buf, dir, name);
    return _anim_loader->get_atlas(path, p);
}

const anim_cell& loader_impl::invalid_anim_atlas()
{
    return _anim_loader->get_invalid_atlas();
}

// Not invalid_anim_atlas() with fps 30, because fps > 0 would make the scenery on it dynamic. 30
// since the missing atlas's frame rate is unknown, and at fps 0 a critter never walks.
const anim_cell& loader_impl::invalid_critter_atlas()
{
    if (!_invalid_critter_atlas) [[unlikely]]
        _invalid_critter_atlas = atlas_loader_traits<class anim_atlas>::make_invalid_atlas(30);
    return *_invalid_critter_atlas;
}

} // namespace floormat::loader_detail
