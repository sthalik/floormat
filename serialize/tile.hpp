#pragma once
#include "src/tile-image.hpp"
#include <nlohmann/json_fwd.hpp>

namespace floormat {

struct local_coords;
struct chunk_coords;
struct chunk_coords_;
struct global_coords;

} // namespace floormat

namespace nlohmann {

template<>
struct adl_serializer<floormat::tile_image_ref> {
    static void to_json(json& j, const floormat::tile_image_ref& val);
    static void from_json(const json& j, floormat::tile_image_ref& val);
};

template<>
struct adl_serializer<floormat::local_coords> {
    static void to_json(json& j, const floormat::local_coords& val);
    static void from_json(const json& j, floormat::local_coords& val);
};

template<>
struct adl_serializer<floormat::chunk_coords> {
    static void to_json(json& j, const floormat::chunk_coords& val);
    static void from_json(const json& j, floormat::chunk_coords& val);
};

template<>
struct adl_serializer<floormat::chunk_coords_> {
    static void to_json(json& j, const floormat::chunk_coords_& val);
    static void from_json(const json& j, floormat::chunk_coords_& val);
};

template<>
struct adl_serializer<floormat::global_coords> {
    static void to_json(json& j, const floormat::global_coords& val);
    static void from_json(const json& j, floormat::global_coords& val);
};

} // namespace nlohmann
