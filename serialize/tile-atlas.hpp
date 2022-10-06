#pragma once
#include "loader.hpp"
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace Magnum::Examples { struct tile_atlas; }

namespace nlohmann {

template<>
struct adl_serializer<std::shared_ptr<Magnum::Examples::tile_atlas>> final {
    static void to_json(json& j, const std::shared_ptr<Magnum::Examples::tile_atlas>& x);
    static void from_json(const json& j, std::shared_ptr<Magnum::Examples::tile_atlas>& x);
};

} // namespace nlohmann
