#pragma once
#include "loader/ground-info.hpp"
#include <nlohmann/json_fwd.hpp>

namespace floormat {

struct ground_info;

} // namespace floormat

namespace nlohmann {

template<>
struct adl_serializer<floormat::ground_info> final {
    static void to_json(json& j, const floormat::ground_info& x) = delete;
    static void from_json(const json& j, floormat::ground_info& x);
};

} // namespace nlohmann
