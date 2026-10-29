#include "impl.hpp"
#include "atlas-loader.inl"
#include "anim-cell.hpp"
#include "anim-traits.hpp"
#include "src/anim-atlas.hpp"
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

// A new copy of invalid_anim_atlas() named `name`, at `fps`. It shares that atlas's sprite, since
// the sprite atlas never frees one.
bptr<anim_atlas> loader_impl::make_placeholder_anim_atlas(StringView name, uint32_t fps)
{
    const auto* sprite = invalid_anim_atlas().atlas->group(rotation::N).sprites[0];
    return atlas_loader_traits<class anim_atlas>::make_invalid_atlas(name, fps, sprite);
}

} // namespace floormat::loader_detail
