#pragma once
#include "compat/int-hash.hpp"
#include "atlas-loader-fwd.hpp"
#include <cr/StringView.h>
#include <cr/Array.h>
#include <cr/Optional.h>
#include <tsl/robin_map.h>

namespace floormat::loader_detail {

template<typename ATLAS, typename TRAITS>
struct atlas_storage
{
    struct string_equals { bool operator()(StringView a, StringView b) const { return a == b; } };

    static_assert(std::is_same_v<typename TRAITS::Atlas, ATLAS>);

    using Traits = TRAITS;
    using Atlas = typename Traits::Atlas;
    using Cell = typename Traits::Cell;

    tsl::robin_map<StringView, size_t, hash_string_view, string_equals> name_map;
    Array<Cell> cell_array;
    Array<String> missing_atlas_names;
    Optional<Cell> invalid_atlas;

    ~atlas_storage() noexcept = default;
};

} // namespace floormat::loader_detail
