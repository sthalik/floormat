#pragma once
#include <nlohmann/json_fwd.hpp>

namespace Magnum::Examples {
struct tile_image;
struct tile;
struct chunk;
} // namespace Magnum::Examples

namespace nlohmann {

template<>
struct adl_serializer<Magnum::Examples::tile_image> {
    static void to_json(json& j, const Magnum::Examples::tile_image& val);
    static void from_json(const json& j, Magnum::Examples::tile_image& val);
};

template<>
struct adl_serializer<Magnum::Examples::tile> {
    static void to_json(json& j, const Magnum::Examples::tile& val);
    static void from_json(const json& j, Magnum::Examples::tile& val);
};

template<>
struct adl_serializer<Magnum::Examples::chunk> {
    static void to_json(json& j, const Magnum::Examples::chunk& val);
    static void from_json(const json& j, Magnum::Examples::chunk& val);
};

} // namespace nlohmann
