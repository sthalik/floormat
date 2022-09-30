#include "serialize.hpp"
#include <nlohmann/json.hpp>
#include "json-magnum.hpp"

std::tuple<big_atlas, bool> big_atlas::from_json(const std::filesystem::path& pathname) noexcept
{

}

bool big_atlas::to_json(const std::filesystem::path& pathname) noexcept
{

}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(big_atlas_tile, position)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(big_atlas_entry, tiles)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(big_atlas, entries)
