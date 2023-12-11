#include "src/tile-atlas.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/magnum-vector.hpp"
#include "loader/loader.hpp"
#include "serialize/pass-mode.hpp"
#include "compat/exception.hpp"
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <nlohmann/json.hpp>

using namespace floormat;

namespace {

struct proxy {
    StringView name;
    Vector2ub size;
    pass_mode passability;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(proxy, name, size)

} // namespace

namespace nlohmann {

void adl_serializer<std::shared_ptr<tile_atlas>>::to_json(json& j, const std::shared_ptr<const tile_atlas>& x)
{
    using nlohmann::to_json;
    if (!x)
        j = nullptr;
    else
        to_json(j, proxy{x->name(), x->num_tiles2(), x->pass_mode()});
}

void adl_serializer<std::shared_ptr<tile_atlas>>::from_json(const json& j, std::shared_ptr<tile_atlas>& val)
{
    if (j.is_null())
        val = nullptr;
    else
    {
        using nlohmann::from_json;
        proxy x;
        from_json(j, x);
        pass_mode p = tile_atlas::default_pass_mode;
        if (j.contains("pass-mode"))
            p = j["pass-mode"];
        val = loader.tile_atlas(x.name, x.size, p);
        if (auto p2 = val->pass_mode(); p2 != p)
        {
            const auto name = val->name();
            fm_throw("atlas {} wrong pass mode {} should be {}"_cf, name, uint8_t(p2), uint8_t(p));
        }
    }
}

} // namespace nlohmann
