#pragma once
#include "src/pass-mode.hpp"
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<>
struct adl_serializer<floormat::pass_mode> {
    static void to_json(json& j, floormat::pass_mode val);
    static void from_json(const json& j, floormat::pass_mode& val);
};

} // namespace nlohmann
