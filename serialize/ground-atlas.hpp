#pragma once
#include "compat/borrowed-ptr.hpp"
#include <nlohmann/json_fwd.hpp>

namespace floormat {

struct ground_def;
struct ground_cell;
class ground_atlas;

} // namespace floormat

namespace nlohmann {

template<>
struct adl_serializer<floormat::ground_def> final {
    static void to_json(json& j, const floormat::ground_def& x);
    static void from_json(const json& j, floormat::ground_def& x);
};

template<>
struct adl_serializer<floormat::bptr<floormat::ground_atlas>> final {
    static void to_json(json& j, const floormat::bptr<const floormat::ground_atlas>& x);
    static void from_json(const json& j, floormat::bptr<floormat::ground_atlas>& x);
};

template<>
struct adl_serializer<floormat::ground_cell> final {
    static void to_json(json& j, const floormat::ground_cell& x);
    static void from_json(const json& j, floormat::ground_cell& x);
};

} // namespace nlohmann
