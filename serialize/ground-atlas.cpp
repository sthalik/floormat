#include "ground-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/magnum-vector.hpp"
#include "loader/loader.hpp"
#include "serialize/pass-mode.hpp"
#include "compat/exception.hpp"
#include <tuple>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <nlohmann/json.hpp>

namespace floormat {

} // namespace floormat

using namespace floormat;

namespace nlohmann {

#if 0
void adl_serializer<ground_info>::to_json(json& j, const ground_info& x)
{
    using nlohmann::to_json;
    j = std::tuple<StringView, Vector2ub, pass_mode>{x.name, x.size, x.pass};
}
#endif

void adl_serializer<ground_info>::from_json(const json& j, ground_info& val)
{
    using nlohmann::from_json;
    val.name = j["name"];
    val.size = j["size"];
    if (j.contains("pass-mode"))
        val.pass = j["pass-mode"];
}

} // namespace nlohmann
