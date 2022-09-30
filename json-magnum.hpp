#pragma once

#include <cstdio>
#include <string>
#include <utility>
#include <fstream>
#include <exception>
#include <nlohmann/json.hpp>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace nlohmann {

template<>
struct adl_serializer<Magnum::Vector2i> final {
    static void to_json(json& j, const Magnum::Vector2i& x);
    static void from_json(const json& j, Magnum::Vector2i& x);
};

void adl_serializer<Magnum::Vector2i>::to_json(json& j, const Magnum::Vector2i& val)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%d x %d", val[0], val[1]);
    j = buf;
}

void adl_serializer<Magnum::Vector2i>::from_json(const json& j, Magnum::Vector2i& val)
{
    std::string str = j;
    int x = 0, y = 0, n = 0;
    int ret = std::sscanf(str.c_str(), "%d x %d%n", &x, &y, &n);
    if (ret != 2 || (std::size_t)n != str.size())
    {
        std::string msg; msg.reserve(64 + str.size());
        msg += "failed to parse string '";
        msg += str;
        msg += "' as Magnum::Vector2i";
        throw std::invalid_argument(msg);
    }
    val = { x, y };
}

} // namespace nlohmann

template<typename t>
struct json_helper final {
    [[nodiscard]] static std::tuple<t, bool> from_json(const std::filesystem::path& pathname) noexcept;
    [[nodiscard]] static bool to_json(const t& self, const std::filesystem::path& pathname) noexcept;
};

template<typename t>
std::tuple<t, bool> json_helper<t>::from_json(const std::filesystem::path& pathname) noexcept {
    using namespace nlohmann;
    using Corrade::Utility::Error;
    std::ifstream s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    try {
        s.open(pathname, std::ios_base::in);
    } catch (const std::ios::failure& e) {
        Error{Error::Flag::NoSpace} << "failed to open " << pathname << ": " << e.what();
        return { {}, false };
    }
    t ret;
    try {
        json j;
        s >> j;
        using nlohmann::from_json;
        from_json(j, ret);
    } catch (const std::exception& e) {
        Error{Error::Flag::NoSpace} << "failed to parse " << pathname << ": " << e.what();
        return { {}, false };
    }
    return { std::move(ret), true };
}

template<typename t>
bool json_helper<t>::to_json(const t& self, const std::filesystem::path& pathname) noexcept {
    using Corrade::Utility::Error;
    try {
        nlohmann::json j = self;

        std::ofstream s;
        s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
        try {
            s.open(pathname, std::ios_base::out | std::ios_base::trunc);
        } catch (const std::ios::failure& e) {
            Error{} << "failed to open" << pathname << "for writing:" << e.what();
            return false;
        }
        s << j.dump(4);
        s.flush();
    } catch (const std::exception& e) {
        Error{Error::Flag::NoSpace} << "failed writing to " << pathname << ": " << e.what();
        return false;
    }

    return true;
}
