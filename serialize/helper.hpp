#pragma once
#include <tuple>
#include <fstream>
#include <exception>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <Corrade/Utility/DebugStl.h>

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
        Error{Error::Flag::NoSpace} << "failed to open '" << pathname << "': " << e.what();
        return { {}, false };
    }
    t ret;
    try {
        json j;
        s >> j;
        using nlohmann::from_json;
        from_json(j, ret);
    } catch (const std::exception& e) {
        Error{Error::Flag::NoSpace} << "failed to parse '" << pathname << "': " << e.what();
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
            Error{Error::Flag::NoSpace} << "failed to open '" << pathname << "' for writing: " << e.what();
            return false;
        }
        s << j.dump(4);
        s.flush();
    } catch (const std::exception& e) {
        Error{Error::Flag::NoSpace} << "failed writing to '" << pathname << "': " << e.what();
        return false;
    }

    return true;
}
