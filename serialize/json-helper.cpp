#include "json-helper.hpp"
#include "compat/assert.hpp"
#include <cerrno>
#include <cstring>
#include <fstream>

namespace floormat {

template<typename T, std::ios_base::openmode mode>
static T open_stream(StringView filename)
{
    T s;
    s.open(filename.data(), mode);
    if (!s)
    {
        char errbuf[128];
        constexpr auto get_error_string = []<std::size_t N> (char (&buf)[N])
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
        fm_error("can't open file '%s' for %s: %s", filename.data(), mode_str, errbuf);
    }
    return s;
}

auto json_helper::from_json_(StringView filename) -> json
{
    json j;
    open_stream<std::ifstream, std::ios_base::in>(filename) >> j;
    return j;
}

void json_helper::to_json_(const json& j, StringView filename, int indent)
{
    (open_stream<std::ofstream, std::ios_base::out>(filename) << j.dump(indent, '\t') << '\n').flush();
}

} // namespace floormat
