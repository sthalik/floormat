#include "json-helper.hpp"
#include <fstream>
#include <filesystem>

namespace floormat {

template<typename T, typename P, std::ios_base::openmode open_mode>
static T open_stream(const std::remove_cvref_t<P>& filename)
{
    T s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    s.open(filename, open_mode);
    return s;
}

auto json_helper::from_json_(const fspath& pathname) -> json
{
    json j;
    open_stream<std::ifstream, fspath, std::ios_base::in>(pathname) >> j;
    return j;
}

void json_helper::to_json_(const json& j, const fspath& pathname, int indent)
{
    (open_stream<std::ofstream, fspath, std::ios_base::out>(pathname) << j.dump(indent, '\t') << '\n').flush();
}

} // namespace floormat
