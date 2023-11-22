#include "json-helper.hpp"
#include "compat/exception.hpp"
#include "compat/strerror.hpp"
#include <concepts>
#include <fstream>
#include <Corrade/Containers/StringStlView.h>

namespace floormat {

namespace {

template<std::derived_from<std::basic_ios<char>> T, std::ios_base::openmode mode>
T open_stream(StringView filename)
{
    T s;
    errno = 0;
    s.open(filename.data(), mode);
    if (!s.good())
    {
        const auto mode_str = (mode & std::ios_base::out) == std::ios_base::out ? "writing"_s : "reading"_s;
        char errbuf[128];
        fm_throw("can't open file '{}' for {}: {}"_cf, filename, mode_str, get_error_string(errbuf));
    }
    return s;
}

} // namespace

auto json_helper::from_json_(StringView filename) noexcept(false) -> json
{
    json j;
    auto s = open_stream<std::ifstream, std::ios_base::in>(filename);
    s >> j;
    if (s.bad() || !s.eof() && s.fail())
    {
        char errbuf[128];
        fm_throw("input/output error while {} '{}': {}"_cf, filename, "reading"_s, get_error_string(errbuf));
    }
    return j;
}

void json_helper::to_json_(const json& j, StringView filename) noexcept(false)
{
    auto s = open_stream<std::ofstream, std::ios_base::out>(filename);
    s << j.dump(2, ' ') << "\n";
    s.flush();
    if (!s.good())
    {
        char errbuf[128];
        fm_throw("input/output error while {} '{}': {}"_cf, filename, "writing"_s, get_error_string(errbuf));
    }
}

} // namespace floormat
