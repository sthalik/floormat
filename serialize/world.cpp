#include "world.hpp"
#include "serialize/tile.hpp"
#include "serialize/tile-atlas.hpp"
#include "src/global-coords.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include <memory>
#include <nlohmann/json.hpp>

#if defined _MSC_VER
#pragma warning(disable : 4996)
#elif defined __GNUG__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace floormat {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(chunk_coords, x, y)

struct global_coords_ final {
    chunk_coords chunk;
    local_coords local;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(global_coords_, chunk, local)

} // namespace floormat

using namespace floormat;

namespace nlohmann {

template<>
struct adl_serializer<std::shared_ptr<chunk>> final {
    static void to_json(json& j, const std::shared_ptr<const chunk>& x);
    static void from_json(const json& j, std::shared_ptr<chunk>& x);
};

void adl_serializer<std::shared_ptr<chunk>>::to_json(json& j, const std::shared_ptr<const chunk>& val)
{
    fm_assert(val);
    using nlohmann::to_json;
    j = *val;
}

void adl_serializer<std::shared_ptr<chunk>>::from_json(const json& j, std::shared_ptr<chunk>& val)
{
    val = std::make_shared<chunk>();
    using nlohmann::from_json;
    *val = j;
}

void adl_serializer<chunk_coords>::to_json(json& j, const chunk_coords& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<chunk_coords>::from_json(const json& j, chunk_coords& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<global_coords>::to_json(json& j, const global_coords& val) { using nlohmann::to_json; to_json(j, global_coords_{val.chunk(), val.local()}); }
void adl_serializer<global_coords>::from_json(const json& j, global_coords& val) { using nlohmann::from_json; global_coords_ x; from_json(j, x); val = {x.chunk, x.local}; }

void adl_serializer<world>::to_json(json& j, const world& val)
{
    using nlohmann::to_json;
    to_json(j, val.chunks());
}

void adl_serializer<world>::from_json(const json& j, world& val)
{
    using T = std::remove_cvref_t<decltype(val.chunks())>;
    T x{};
    using nlohmann::from_json;
    from_json(j, x);
    val = world{std::move(x)};
}

} // namespace nlohmann
