#pragma once
#include "src/wall-atlas.hpp"
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<>
struct adl_serializer<std::shared_ptr<floormat::wall_atlas>>
{
    static void to_json(json& j, const std::shared_ptr<const floormat::wall_atlas>& x);
    static void from_json(const json& j, std::shared_ptr<floormat::wall_atlas>& x);
};

} // namespace nlohmann

namespace floormat::Wall::detail {

uint8_t direction_index_from_name(StringView s);
StringView direction_index_to_name(size_t i);
[[nodiscard]] Group read_group_metadata(const nlohmann::json& jgroup);
[[nodiscard]] Direction read_direction_metadata(const nlohmann::json& jroot, Direction_ dir);
Info read_info_header(const nlohmann::json& jroot);

void write_group_metadata(nlohmann::json& jgroup, const Group& val);

} // namespace floormat::Wall::detail
