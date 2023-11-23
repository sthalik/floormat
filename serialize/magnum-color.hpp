#pragma once
#include <Magnum/Math/Math.h>
#include <nlohmann/json_fwd.hpp>

template<>
struct nlohmann::adl_serializer<Magnum::Color3> {
    static void to_json(json& j, const Magnum::Color3& val);
    static void from_json(const json& j, Magnum::Color3& val);
};

template<>
struct nlohmann::adl_serializer<Magnum::Color4> {
    static void to_json(json& j, const Magnum::Color4& val);
    static void from_json(const json& j, Magnum::Color4& val);
};

template<>
struct nlohmann::adl_serializer<Magnum::Color3ub> {
    static void to_json(json& j, const Magnum::Color3ub& val);
    static void from_json(const json& j, Magnum::Color3ub& val);
};

template<>
struct nlohmann::adl_serializer<Magnum::Color4ub> {
    static void to_json(json& j, const Magnum::Color4ub& val);
    static void from_json(const json& j, Magnum::Color4ub& val);
};
