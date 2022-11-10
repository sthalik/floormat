#include "json-helper.hpp"
#include <fstream>

namespace floormat {

template<typename T, std::ios_base::openmode mode>
static T open_stream(StringView filename)
{
    T s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    s.open(filename, mode);
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
