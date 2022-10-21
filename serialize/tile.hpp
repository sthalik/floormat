#pragma once
#include <nlohmann/json_fwd.hpp>

namespace floormat {

struct tile_image;
struct tile;
struct chunk;
struct local_coords;

} // namespace floormat

namespace nlohmann {

template<>
struct adl_serializer<floormat::tile_image> {
    static void to_json(json& j, const floormat::tile_image& val);
    static void from_json(const json& j, floormat::tile_image& val);
};

template<>
struct adl_serializer<floormat::tile> {
    static void to_json(json& j, const floormat::tile& val);
    static void from_json(const json& j, floormat::tile& val);
};

template<>
struct adl_serializer<floormat::chunk> {
    static void to_json(json& j, const floormat::chunk& val);
    static void from_json(const json& j, floormat::chunk& val);
};

template<>
struct adl_serializer<floormat::local_coords> {
    static void to_json(json& j, const floormat::local_coords& val);
    static void from_json(const json& j, floormat::local_coords& val);
};

} // namespace nlohmann
