#pragma once
#include "atlas-loader-fwd.hpp"
#include "string-index-map.hpp"
#include <cr/StringView.h>
#include <cr/Array.h>
#include <cr/Optional.h>

namespace floormat::loader_detail {

template<typename ATLAS, typename TRAITS>
struct atlas_storage
{
    static_assert(std::is_same_v<typename TRAITS::Atlas, ATLAS>);

    using Traits = TRAITS;
    using Atlas = typename Traits::Atlas;
    using Cell = typename Traits::Cell;

    string_index_map name_map;
    Array<Cell> cell_array;
    Array<String> missing_atlas_names;
    Optional<Cell> invalid_atlas;

    ~atlas_storage() noexcept = default;
};

} // namespace floormat::loader_detail
