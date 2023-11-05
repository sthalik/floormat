#pragma once
#include "compat/assert.hpp"
#include <cstdio>
#include <string>
#include <Magnum/Math/Vector2.h>
#include <nlohmann/json_fwd.hpp>

namespace floormat::Serialize {
    [[noreturn]] void throw_failed_to_parse_vector2(const std::string& str);
    [[noreturn]] void throw_vector2_overflow(const std::string& str);
}

namespace nlohmann {

template<typename T>
requires std::is_integral_v<T>
struct adl_serializer<Magnum::Math::Vector2<T>> final
{
    static void to_json(json& j, const Magnum::Math::Vector2<T>& val);
    static void from_json(const json& j, Magnum::Math::Vector2<T>& val);
};

} // namespace nlohmann
