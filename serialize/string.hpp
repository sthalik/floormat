#pragma once

#include <nlohmann/json_fwd.hpp>

namespace Corrade::Containers {

template<typename T> class BasicStringView;
class String;
using StringView = BasicStringView<const char>;

} // namespace Corrade::Containers

namespace nlohmann {

template<>
struct adl_serializer<Corrade::Containers::String> {
    static void to_json(json& j, const Corrade::Containers::String& val);
    static void from_json(const json& j, Corrade::Containers::String& val);
};

template<>
struct adl_serializer<Corrade::Containers::StringView> {
    static void to_json(json& j, const Corrade::Containers::StringView& val);
    static void from_json(const json& j, Corrade::Containers::StringView& val);
};

} // namespace nlohmann
