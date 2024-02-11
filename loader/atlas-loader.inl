#pragma once
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "compat/os-file.hpp"
#include "atlas-loader.hpp"
#include "atlas-loader-storage.hpp"
#include "loader/loader.hpp"
#include <memory>
#include <cr/ArrayView.h>
#include <cr/Optional.h>
#include <cr/Move.h>

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
    if (!s.name_map.empty()) [[likely]]
        return { s.cell_array.data(), s.cell_array.size() };

    t.load_atlas_list(s);

    for (Cell& c : s.cell_array)
    {
        String& name{t.name_of(c)};
        if (name.isSmall())
            name = String{AllocatedInit, name};
        fm_soft_assert(name != loader.INVALID);
        fm_soft_assert(loader.check_atlas_name(name));
    }

    s.name_map.max_load_factor(0.4f);
    if (!s.cell_array.empty())
        s.name_map.reserve(s.cell_array.size()*5/2 + 1);
    for (auto i = 0uz; const auto& c : s.cell_array)
        s.name_map[t.name_of(c)] = i++;

    { const Cell& invalid_atlas{get_invalid_atlas()};
      size_t sz{s.cell_array.size()};
      s.cell_array.push_back(invalid_atlas);
      s.name_map[loader.INVALID] = sz;
    }

    for (const auto& [name, index] : s.name_map)
        fm_assert(index < s.cell_array.size() || index == -1uz);

    fm_debug_assert(!s.name_map.empty());
    fm_debug_assert(!s.cell_array.empty());

    return { s.cell_array.data(), s.cell_array.size() };
}

template<typename ATLAS, typename TRAITS>
const std::shared_ptr<ATLAS>& atlas_loader<ATLAS, TRAITS>::get_atlas(StringView name, loader_policy p)
{
    ensure_atlas_list();
    const std::shared_ptr<Atlas>& invalid_atlas = t.atlas_of(get_invalid_atlas());

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
            return invalid_atlas;
        }

    fm_soft_assert(loader.check_atlas_name(name));

    if (auto it = s.name_map.find(name); it != s.name_map.end()) [[likely]]
    {
        if (it->second == -1uz) [[unlikely]]
        {
            switch (p)
            {
            using enum loader_policy;
            case error:
                goto error;
            case warn:
            case ignore:
                return invalid_atlas;
            }
        }
        else
        {
            Cell& c = s.cell_array[it->second];
            fm_assert(t.name_of(c));
            std::shared_ptr<ATLAS>& atlas{t.atlas_of(c)};
            if (!atlas) [[unlikely]]
            {
                atlas = make_atlas(name, c);
                fm_assert(atlas);
                fm_assert(t.atlas_of(c) == atlas);
                fm_assert(t.name_of(*atlas) == name);
                return atlas;
            }
            else
            {
                fm_debug_assert(t.name_of(c) == name);
                return atlas;
            }
        }
    }
    else if (Optional<Cell> c_{t.make_cell(name)})
    {
        fm_assert(t.name_of(*c_));
        fm_assert(!t.atlas_of(*c_));
        fm_assert(t.name_of(*c_) == name);
        const size_t index{s.cell_array.size()};
        if (t.name_of(*c_).isSmall())
            t.name_of(*c_) = String{ AllocatedInit, t.name_of(*c_) };
        s.cell_array.emplace_back(Utility::move(*c_));
        Cell& c{s.cell_array.back()};
        t.atlas_of(c) = make_atlas(name, c);
        fm_debug_assert(t.atlas_of(c));
        s.name_map[t.name_of(c)] = index;
        return t.atlas_of(c);
    }
    else
    {
        switch (p)
        {
        using enum loader_policy;
        case error:
            goto error;
        case warn:
            goto missing_warn;
        case ignore:
            return invalid_atlas;
        }
    }

    std::unreachable();
    fm_assert(false);

error:
    fm_throw("no such atlas '{}'"_cf, name);

missing_warn:
    s.missing_atlas_names.push_back(String { AllocatedInit, name });
    s.name_map[ s.missing_atlas_names.back() ] = -1uz;

    if (name != loader.INVALID)
        DBG_nospace << t.loader_name() << " '" << name << "' doesn't exist";

    return invalid_atlas;
}

template<typename ATLAS, typename TRAITS>
auto atlas_loader<ATLAS, TRAITS>::make_atlas(StringView name, const Cell& c) -> std::shared_ptr<Atlas>
{
    fm_assert(name != "<invalid>"_s);
    fm_soft_assert(!t.name_of(c) || t.name_of(c) == name);
    fm_soft_assert(loader.check_atlas_name(name));
    return t.make_atlas(name, c);
}

template<typename ATLAS, typename TRAITS>
auto atlas_loader<ATLAS, TRAITS>::get_invalid_atlas() -> const Cell&
{
    if (s.invalid_atlas) [[likely]]
        return *s.invalid_atlas;
    s.invalid_atlas = t.make_invalid_atlas(s);
    fm_assert(s.invalid_atlas);
    fm_assert(t.atlas_of(*s.invalid_atlas));
    fm_assert(t.name_of(*s.invalid_atlas) == loader.INVALID);
    return *s.invalid_atlas;
}

template<typename ATLAS, typename TRAITS>
bool atlas_loader<ATLAS, TRAITS>::cell_exists(Corrade::Containers::StringView name)
{
    return s.name_map.contains(name);
}

template<typename ATLAS, typename TRAITS>
auto atlas_loader<ATLAS, TRAITS>::get_cell(StringView name) -> const Cell&
{
    auto it = s.name_map.find(name);
    fm_assert(it != s.name_map.end());
    return s.cell_array[ it->second ];
}

template<typename ATLAS, typename TRAITS>
void atlas_loader<ATLAS, TRAITS>::register_cell(Cell&& c)
{
    String& name{t.name_of(c)};
    if (name.isSmall())
        name = String{AllocatedInit, name};
    const std::shared_ptr<Atlas>& atlas{t.atlas_of(c)};
    fm_assert(!s.name_map.contains(name));
    fm_soft_assert(loader.check_atlas_name(name));
    fm_assert(!atlas || t.name_of(*atlas) == name);
    const size_t index{s.cell_array.size()};
    s.cell_array.push_back(Utility::move(c));
    s.name_map[ t.name_of(s.cell_array.back()) ] = index;
}

} // namespace floormat::loader_detail
