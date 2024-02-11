#pragma once
#include "compat/int-hash.hpp"
#include "atlas-loader-fwd.hpp"
#include <vector>
#include <cr/StringView.h>
#include <tsl/robin_map.h>

namespace floormat::loader_detail {

template<typename ATLAS, typename TRAITS>
struct atlas_storage
{
    static_assert(std::is_same_v<typename TRAITS::Atlas, ATLAS>);

    using Traits = TRAITS;
    using Atlas = typename TRAITS::Atlas;
    using Cell = typename TRAITS::Cell;

    tsl::robin_map<StringView, size_t, hash_string_view> name_map;
    std::vector<Cell> cell_array;
    std::vector<String> missing_atlas_names;
    Pointer<Cell> invalid_atlas;

    ~atlas_storage() noexcept = default;
};

} // namespace floormat::loader_detail
