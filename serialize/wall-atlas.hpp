#pragma once
#include "src/wall-atlas.hpp"
#include <bitset>
#include <memory>
#include <Corrade/Containers/Array.h>
#include <nlohmann/json_fwd.hpp>

namespace floormat::Wall::detail {

using nlohmann::json;

[[nodiscard]] Array<Frame> read_all_frames(const json& jroot);
[[nodiscard]] Group read_group_metadata(const json& jgroup);
[[nodiscard]] Direction read_direction_metadata(const json& jroot, Direction_ dir);
Info read_info_header(const json& jroot);

void write_all_frames(json& jroot, ArrayView<const Frame> array);
void write_group_metadata(json& jgroup, const Group& val);
void write_direction_metadata(json& jdir, const Direction& dir);
void write_all_directions(json& jroot, const wall_atlas& a);
void write_info_header(json& jroot, const Info& info);
bool is_direction_defined(const Direction& dir);

} // namespace floormat::Wall::detail
