#pragma once
#include "../tile-atlas.hpp"
#include <nlohmann/json.hpp>

namespace nlohmann {

template<>
struct adl_serializer<Magnum::Examples::tile_atlas> final {
    static void to_json(json& j, const Magnum::Examples::tile_atlas& x);
    static void from_json(const json& j, Magnum::Examples::tile_atlas& x);
};

void adl_serializer<Magnum::Examples::tile_atlas>::to_json(json& j, const Magnum::Examples::tile_atlas& x)
{
}

void adl_serializer<Magnum::Examples::tile_atlas>::from_json(const json& j, Magnum::Examples::tile_atlas& x)
{
}

} // namespace nlohmann

namespace Magnum::Examples::Serialize {



} // namespace Magnum::Examples::Serialize
