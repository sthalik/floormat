#include "impl.hpp"
#include "atlas-loader.inl"
#include "ground-traits.hpp"
#include "ground-cell.hpp"
#include "compat/borrowed-ptr.inl"
#include <Magnum/Math/Vector2.h>

namespace floormat::loader_detail {

template class atlas_loader<ground_atlas>;

bptr<ground_atlas> loader_impl::get_ground_atlas(StringView name, Vector2ub size, pass_mode pass) noexcept(false)
{
    return _ground_loader->make_atlas(name, {
        .atlas = {}, .name = {}, .size = size, .pass = pass,
    });
}

atlas_loader<class ground_atlas>* loader_impl::make_ground_atlas_loader()
{
    return new atlas_loader<class ground_atlas>;
}

auto loader_impl::ground_atlas_list() noexcept(false) -> ArrayView<const ground_cell>
{
    return _ground_loader->atlas_list();
}

const ground_cell& loader_impl::invalid_ground_atlas()
{
    return _ground_loader->get_invalid_atlas();
}

const bptr<class ground_atlas>& loader_impl::ground_atlas(StringView filename, loader_policy policy) noexcept(false)
{
    return _ground_loader->get_atlas(filename, policy);
}

} // namespace floormat::loader_detail
