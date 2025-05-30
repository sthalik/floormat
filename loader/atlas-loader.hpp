#pragma once
#include "compat/defs.hpp"
#include "atlas-loader-fwd.hpp"
#include "policy.hpp"

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
    using AtlasPtr = std::decay_t<decltype(t.atlas_of(std::declval<const Cell&>()))>;

    ~atlas_loader() noexcept = default;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(atlas_loader);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(atlas_loader);

    atlas_loader(TRAITS&& traits);
    atlas_loader() requires std::is_default_constructible_v<TRAITS>;

    ArrayView<const Cell> atlas_list();
    const AtlasPtr& get_atlas(StringView name, loader_policy p);
    AtlasPtr make_atlas(StringView name, const Cell& cell);

    bool cell_exists(StringView name);
    const Cell& get_cell(StringView name);
    void register_cell(Cell&& c);

    const Cell& get_invalid_atlas();
};

} // namespace floormat::loader_detail
