#include "string-index-map.hpp"
#include "compat/hash.hpp"
#include "compat/hash-table-load-factor.hpp"
#include <gtl/phmap.hpp>

namespace floormat::loader_detail {

struct string_index_map::impl
{
    struct string_equals { bool operator()(StringView a, StringView b) const { return a == b; } };

    gtl::flat_hash_map<StringView, usize, hash_string_view, string_equals> map;
};

string_index_map::string_index_map() : p{InPlaceInit} {}
string_index_map::~string_index_map() noexcept = default;

bool string_index_map::empty() const noexcept { return p->map.empty(); }
usize string_index_map::size() const noexcept { return p->map.size(); }
bool string_index_map::contains(StringView name) const { return p->map.contains(name); }
usize& string_index_map::operator[](StringView name) { return p->map[name]; }
void string_index_map::clear() { p->map.clear(); }
void string_index_map::set_load_factor(usize element_count) { Hash::set_open_addressing_load_factor(p->map, element_count); }
void string_index_map::set_load_factor() { Hash::set_open_addressing_load_factor(p->map); }

const usize* string_index_map::find(StringView name) const
{
    const auto it = p->map.find(name);
    return it != p->map.end() ? &it->second : nullptr;
}

bool string_index_map::all_indices_below(usize limit) const
{
    for (const auto& [name, index] : p->map)
        if (index >= limit && index != -1uz)
            return false;
    return true;
}

} // namespace floormat::loader_detail
