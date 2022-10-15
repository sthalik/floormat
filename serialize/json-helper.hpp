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
    static t from_json(const std::filesystem::path& pathname);

    template<typename t>
    static void to_json(const t& self, const std::filesystem::path& pathname);
};

template<typename t>
t json_helper::from_json(const std::filesystem::path& pathname)
{
    using Corrade::Utility::Error;
    std::ifstream s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    s.open(pathname, std::ios_base::in);
    t ret;
    nlohmann::json j;
    s >> j;
    ret = j;
    return ret;
}

template<typename t>
void json_helper::to_json(const t& self, const std::filesystem::path& pathname)
{
    using Corrade::Utility::Error;
    nlohmann::json j = self;

    std::ofstream s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    s.open(pathname, std::ios_base::out | std::ios_base::trunc);
    s << j.dump(4);
    s << '\n';
    s.flush();
}
