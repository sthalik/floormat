#pragma once
#include <nlohmann/json_fwd.hpp>

namespace floormat {

struct chunk_coords;
struct global_coords;
struct chunk;
struct world;

} // namespace floormat

namespace nlohmann {

template<>
struct adl_serializer<floormat::chunk_coords> {
    static void to_json(json& j, const floormat::chunk_coords& val);
    static void from_json(const json& j, floormat::chunk_coords& val);
};

template<>
struct adl_serializer<floormat::global_coords> {
    static void to_json(json& j, const floormat::global_coords& val);
    static void from_json(const json& j, floormat::global_coords& val);
};

} // namespace nlohmann

