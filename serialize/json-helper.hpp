#pragma once
#include <tuple>
#include <fstream>
#include <exception>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <Corrade/Utility/DebugStl.h>

struct json_helper final {
    template<typename t>
    [[nodiscard]]
     static std::tuple<t, bool> from_json(const std::filesystem::path& pathname);

    template<typename t>
    [[nodiscard]]
    static bool to_json(const t& self, const std::filesystem::path& pathname);
};

template<typename t>
std::tuple<t, bool> json_helper::from_json(const std::filesystem::path& pathname) {
    using Corrade::Utility::Error;
    std::ifstream s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    try {
        s.open(pathname, std::ios_base::in);
    } catch (const std::ios::failure& e) {
        Error{} << "failed to open" << pathname << "for reading:" << e.what();
        return {};
    }
    t ret;
    nlohmann::json j;
    s >> j;
    ret = j;
    return { std::move(ret), true };
}

template<typename t>
bool json_helper::to_json(const t& self, const std::filesystem::path& pathname) {
    using Corrade::Utility::Error;
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
    s << '\n';
    s.flush();
    return true;
}
