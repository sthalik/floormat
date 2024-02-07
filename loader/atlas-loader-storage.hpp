#pragma once
#include "compat/int-hash.hpp"
#include "atlas-loader-fwd.hpp"
#include <vector>
#include <tsl/robin_map.h>

namespace floormat::loader_detail {

template<typename ATLAS, typename TRAITS>
struct atlas_storage
{
    using Traits = TRAITS;
    using Cell = typename TRAITS::Cell;

    tsl::robin_map<StringView, Cell*, hash_string_view> name_map;
    std::vector<Cell> cell_array;
    std::vector<String> missing_atlas_names;
    Pointer<Cell> invalid_atlas;

    bool is_empty() const { return cell_array.empty(); }
};

} // namespace floormat::loader_detail
