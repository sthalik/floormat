#pragma once
#include "src/wall-atlas.hpp"
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<>
struct adl_serializer<floormat::Wall::Frame>
{
    static void to_json(json& j, const floormat::Wall::Frame& x);
    static void from_json(const json& j, floormat::Wall::Frame& x);
};

template<>
struct adl_serializer<std::shared_ptr<floormat::wall_atlas>>
{
    static void to_json(json& j, const std::shared_ptr<const floormat::wall_atlas>& x);
    static void from_json(const json& j, std::shared_ptr<floormat::wall_atlas>& x);
};

} // namespace nlohmann

namespace floormat::Wall::detail {

using nlohmann::json;

uint8_t direction_index_from_name(StringView s);
StringView direction_index_to_name(size_t i);

[[nodiscard]] Array<Frame> read_all_frames(const json& jroot);
[[nodiscard]] Group read_group_metadata(const json& jgroup);
[[nodiscard]] Direction read_direction_metadata(const json& jroot, Direction_ dir);
Pair<Array<Direction>, std::array<DirArrayIndex, 4>> read_all_directions(const json& jroot);
Info read_info_header(const json& jroot);

void write_all_frames(json& jroot, ArrayView<const Frame> array);
void write_group_metadata(json& jgroup, const Group& val);
void write_direction_metadata(json& jdir, const Direction& dir);
void write_all_directions(json& jroot, const wall_atlas& a);
void write_info_header(json& jroot, const Info& info);

} // namespace floormat::Wall::detail
