#pragma once
#include "compat/defs.hpp"
#include "atlas-loader-fwd.hpp"
#include "policy.hpp"
#include <memory>

namespace floormat::loader_detail {

template<typename ATLAS, typename TRAITS>
class atlas_loader final
{
    [[fm_no_unique_address]] TRAITS t;
    atlas_storage<ATLAS, TRAITS> s;

public:
    using Traits = TRAITS;
    using Atlas = ATLAS;
    using Cell = typename TRAITS::Cell;

    ~atlas_loader() noexcept = default;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(atlas_loader);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(atlas_loader);

    atlas_loader(TRAITS&& traits);
    atlas_loader() requires std::is_default_constructible_v<TRAITS>;

    ArrayView<const Cell> ensure_atlas_list();
    const std::shared_ptr<Atlas>& get_atlas(StringView name, loader_policy p);
    const Cell& get_invalid_atlas();
};

} // namespace floormat::loader_detail
