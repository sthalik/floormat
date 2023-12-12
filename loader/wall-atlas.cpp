#include "loader/impl.hpp"
#include "wall-info.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "wall-info.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/corrade-string.hpp"
#include "src/tile-defs.hpp"
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <vector>

namespace floormat {

using nlohmann::json;

[[maybe_unused]] static void from_json(const json& j, wall_info& val)
{
    val = {};
    val.name = j["name"];
    fm_soft_assert(loader.check_atlas_name(val.name));
    if (j.contains("descr"))
        val.descr = j["descr"];
}

[[maybe_unused]] static void to_json(json& j, const wall_info& val)
{
    j["name"] = val.name;
    if (val.descr)
        j["descr"] = val.descr;
}

} // namespace floormat

namespace floormat::loader_detail {

std::shared_ptr<wall_atlas> loader_impl::get_wall_atlas(StringView name, StringView path)
{
    auto def = wall_atlas_def::deserialize(""_s.join({path, ".json"_s}));
    auto tex = texture(""_s, path);

    fm_soft_assert(name == def.header.name);
    fm_soft_assert(!def.frames.empty());
    auto atlas = std::make_shared<class wall_atlas>(std::move(def), path, tex);
    return atlas;
}

const wall_info& loader_impl::make_invalid_wall_atlas()
{
    if (invalid_wall_atlas) [[likely]]
        return *invalid_wall_atlas;

    constexpr auto name = "<invalid>"_s;
    constexpr auto frame_size = Vector2ui{iTILE_SIZE.x(), iTILE_SIZE.z()};

    auto a = std::make_shared<class wall_atlas>(
        wall_atlas_def {
            Wall::Info{.name = name, .depth = 8},
            {{ {}, frame_size}, },
            {{ {.index = 0, .count = 1, .pixel_size = frame_size, } } },
            {{ {.val = 0}, {}, }},
            {1u},
        }, name, make_error_texture());
    invalid_wall_atlas = Pointer<wall_info>{InPlaceInit, wall_info{ .name = name, .atlas = std::move(a) } };
    return *invalid_wall_atlas;
}

std::shared_ptr<class wall_atlas> loader_impl::wall_atlas(StringView name, bool fail_ok)
{
    fm_soft_assert(check_atlas_name(name));
    char buf[FILENAME_MAX];
    auto path = make_atlas_path(buf, loader.WALL_TILESET_PATH, name);

    auto it = wall_atlas_map.find(name);
    if (it == wall_atlas_map.end())
    {
        if (!fail_ok)
            fm_throw("no such wall atlas '{}'"_cf, name);
        else
            return make_invalid_wall_atlas().atlas;
    }
    fm_assert(it->second != nullptr);
    if (!it->second->atlas) [[unlikely]]
        it->second->atlas = get_wall_atlas(it->second->name, path);
    return it->second->atlas;
}

void loader_impl::get_wall_atlas_list()
{
    wall_atlas_array = json_helper::from_json<std::vector<wall_info>>(Path::join(WALL_TILESET_PATH, "walls.json"_s));
    wall_atlas_array.shrink_to_fit();
    wall_atlas_map.clear();
    wall_atlas_map.reserve(wall_atlas_array.size()*2);

    for (auto& x : wall_atlas_array)
    {
        fm_soft_assert(check_atlas_name(x.name));
        StringView name = x.name;
        wall_atlas_map[name] = &x;
        fm_debug_assert(name.data() == wall_atlas_map[name]->name.data());
    }

    fm_assert(!wall_atlas_map.empty());
}

ArrayView<const wall_info> loader_impl::wall_atlas_list()
{
    if (wall_atlas_map.empty())
        get_wall_atlas_list();
    fm_assert(!wall_atlas_map.empty());
    return wall_atlas_array;
}

} // namespace floormat::loader_detail
