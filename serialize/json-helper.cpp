#include "json-helper.hpp"
#include "compat/exception.hpp"
#include <cerrno>
#include <cstring>
#include <fstream>
#include <Corrade/Containers/StringStlView.h>

namespace floormat {

template<typename T, std::ios_base::openmode mode>
static T open_stream(StringView filename)
{
    T s;
    s.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    s.open(filename.data(), mode);
    if (!s)
    {
        char errbuf[128];
        constexpr auto get_error_string = []<size_t N> (char (&buf)[N])
        {
            buf[0] = '\0';
#ifndef _WIN32
            (void)::strerror_r(errno, buf, std::size(buf));
#else
            (void)::strerror_s(buf, std::size(buf), errno);
#endif
        };
        const char* mode_str = (mode & std::ios_base::out) == std::ios_base::out ? "writing" : "reading";
        (void)get_error_string(errbuf);
        fm_throw("can't open file '{}' for {}: {}"_cf, filename, mode_str, errbuf);
    }
    return s;
}

auto json_helper::from_json_(StringView filename) noexcept(false) -> json
{
    json j;
    open_stream<std::ifstream, std::ios_base::in>(filename) >> j;
    return j;
}

void json_helper::to_json_(const json& j, StringView filename) noexcept(false)
{
    (open_stream<std::ofstream, std::ios_base::out>(filename) << j.dump(2, ' ') << '\n').flush();
}

} // namespace floormat
