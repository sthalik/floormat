#pragma once
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "atlas-loader.hpp"
#include "atlas-loader-storage.hpp"
#include "loader/loader.hpp"
#include <memory>
#include <Corrade/Containers/ArrayView.h>

namespace floormat::loader_detail {

template<typename ATLAS, typename TRAITS>
atlas_loader<ATLAS, TRAITS>::atlas_loader(TRAITS&& traits): // NOLINT(*-rvalue-reference-param-not-moved)
      t{Utility::move(traits)}
{}

template<typename ATLAS, typename TRAITS>
atlas_loader<ATLAS, TRAITS>::atlas_loader() requires std::is_default_constructible_v<TRAITS>: atlas_loader{TRAITS{}} {}

template<typename TRAITS>
atlas_loader(TRAITS&& traits) noexcept -> atlas_loader<TRAITS, typename TRAITS::Atlas>;

template<typename ATLAS, typename TRAITS>
auto atlas_loader<ATLAS, TRAITS>::ensure_atlas_list() -> ArrayView<const Cell>
{
    if (!s.cell_array.empty()) [[likely]]
        return { s.cell_array.data(), s.cell_array.size() };
    t.ensure_atlases_loaded(s);
    fm_assert(!s.cell_array.empty());
    return { s.cell_array.data(), s.cell_array.size() };
}

template<typename ATLAS, typename TRAITS>
const std::shared_ptr<ATLAS>& atlas_loader<ATLAS, TRAITS>::get_atlas(StringView name, loader_policy p)
{
    ensure_atlas_list();
    auto* const invalid_atlas = const_cast<Cell*>(&t.make_invalid_atlas(s));
    fm_debug_assert(invalid_atlas);
    fm_debug_assert(t.atlas_of(*invalid_atlas));

    switch (p)
    {
    using enum loader_policy;
    case error:
    case ignore:
    case warn:
        break;
    default:
        fm_abort("invalid loader_policy (%d)", (int)p);
    }

    if (name == loader.INVALID) [[unlikely]]
        switch (p)
        {
        using enum loader_policy;
        case error:
            goto error;
        case ignore:
        case warn:
            return t.atlas_of(*invalid_atlas);
        }

    fm_soft_assert(loader.check_atlas_name(name));

    if (auto it = s.name_map.find(name); it != s.name_map.end()) [[likely]]
    {
        if (it->second == invalid_atlas)
        {
            switch (p)
            {
            using enum loader_policy;
            case error:
                goto error;
            case warn:
            case ignore:
                return t.atlas_of(*invalid_atlas);
            }
        }
        else if (const auto& atlas = t.atlas_of(*it->second))
            return t.atlas_of(*it->second);
        else
            return t.atlas_of(*it->second) = t.make_atlas(name, *it->second);
    }
    else
        switch (p)
        {
        using enum loader_policy;
        case error:
            goto error;
        case warn:
            goto missing_warn;
        case ignore:
            return t.atlas_of(*invalid_atlas);
        }

    std::unreachable();
    fm_assert(false);

error:
    fm_throw("no such atlas '{}'"_cf, name);

missing_warn:
    s.missing_atlas_names.push_back(String { AllocatedInit, name });
    s.name_map[ s.missing_atlas_names.back() ] = invalid_atlas;

    if (name != loader.INVALID)
        DBG_nospace << t.loader_name() << " '" << name << "' doesn't exist";

    return t.atlas_of(*invalid_atlas);
}

template<typename ATLAS, typename TRAITS>
auto atlas_loader<ATLAS, TRAITS>::make_atlas(StringView name, const Cell& c) -> std::shared_ptr<Atlas>
{
    fm_assert(name != "<invalid>"_s);
    fm_soft_assert(!c.name || t.name_of(c) == name);
    fm_soft_assert(loader.check_atlas_name(name));
    return t.make_atlas(name, c);
}

template<typename ATLAS, typename TRAITS>
auto atlas_loader<ATLAS, TRAITS>::get_invalid_atlas() -> const Cell&
{
    const auto& cell = t.make_invalid_atlas(s);
    fm_assert(t.atlas_of(cell));
    return cell;
}

} // namespace floormat::loader_detail
