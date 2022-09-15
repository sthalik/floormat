#pragma once
//#define GAME_REAL_JSON
#if !defined __CLION_IDE__ || defined GAME_REAL_JSON
#   include <nlohmann/json.hpp>
#else

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "NotImplementedFunctions"

namespace nlohmann {
    struct json {
        template<typename t> operator t() const;
        const json& dump(int) const;
        template<typename t> json(const t&);
        json();
    };
    template<typename t> json& operator>>(const t&, json&);
    template<typename t> json& operator<<(t&, const json&);
    template<typename t> struct adl_serializer {
        static void to_json(json&, const t&);
        static void from_json(const json&, t&);
    };
    template<typename t> void from_json(const json&, t&);
    template<typename t> void to_json(const json&, const t&);
} // namespace nlohmann

#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(...)

#pragma clang diagnostic pop
#endif
