#include "loader/impl.hpp"
#include "wall-info.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/corrade-string.hpp"
#include "src/tile-defs.hpp"
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>

namespace floormat {

using nlohmann::json;
using loader_detail::loader_impl;

[[maybe_unused]] static void from_json(const json& j, wall_info& val)
{
    val = {};
    val.name = j["name"];
    fm_soft_assert(loader.check_atlas_name(val.name));
}

[[maybe_unused]] static void to_json(json& j, const wall_info& val)
{
    j["name"] = val.name;
}

} // namespace floormat

namespace floormat::loader_detail {

std::shared_ptr<wall_atlas> loader_impl::get_wall_atlas(StringView name, StringView dir)
{
    fm_assert(name != "<invalid>"_s);
    char buf[FILENAME_MAX];
    auto filename = make_atlas_path(buf, dir, name);
    auto def = wall_atlas_def::deserialize(""_s.join({filename, ".json"_s}));
    auto tex = texture(""_s, filename);

    fm_soft_assert(name == def.header.name);
    fm_soft_assert(!def.frames.isEmpty());
    auto atlas = std::make_shared<class wall_atlas>(std::move(def), dir, tex);
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
            array<Wall::Frame>({{ {}, frame_size}, }),
            array<Wall::Direction>({{ {.index = 0, .count = 1, .pixel_size = frame_size, .is_defined = true, } } }),
            {{ {.val = 0}, {}, }},
            {1u},
        }, name, make_error_texture(frame_size));
    invalid_wall_atlas = Pointer<wall_info>{InPlaceInit, wall_info{ .name = name, .atlas = std::move(a) } };
    return *invalid_wall_atlas;
}

std::shared_ptr<class wall_atlas> loader_impl::wall_atlas(StringView name, loader_policy policy) noexcept(false)
{
    (void)wall_atlas_list();

    switch (policy)
    {
    case loader_policy::error:
        fm_assert(name != INVALID);
        break;
    case loader_policy::ignore:
    case loader_policy::warn:
        break;
    default:
        fm_abort("invalid loader_policy");
    }

    fm_soft_assert(check_atlas_name(name));
    auto it = wall_atlas_map.find(name);

    if (it != wall_atlas_map.end()) [[likely]]
    {
        if (it->second == (wall_info*)-1) [[unlikely]]
        {
            switch (policy)
            {
            case loader_policy::error:
                goto error;
            case loader_policy::warn:
            case loader_policy::ignore:
                goto missing_ok;
            }
            std::unreachable();
        }
        else if (!it->second->atlas)
            return it->second->atlas = get_wall_atlas(name, loader.WALL_TILESET_PATH);
        else
            return it->second->atlas;
    }
    else
    {
        switch (policy)
        {
        case loader_policy::error:
            goto error;
        case loader_policy::warn:
            goto missing_warn;
        case loader_policy::ignore:
            goto missing_ok;
        }
        std::unreachable();
    }

missing_warn:
    {
        missing_wall_atlases.push_back(String { AllocatedInit, name });
        auto string_view = StringView{missing_wall_atlases.back()};
        wall_atlas_map[string_view] = (wall_info*)-1;
    }

    if (name != "<invalid>")
        DBG_nospace << "wall_atlas '" << name << "' doesn't exist";

missing_ok:
    return make_invalid_wall_atlas().atlas;

error:
    fm_throw("no such wall atlas '{}'"_cf, name);
}

void loader_impl::get_wall_atlas_list()
{
    fm_assert(wall_atlas_map.empty());

    wall_atlas_array = json_helper::from_json<std::vector<wall_info>>(Path::join(WALL_TILESET_PATH, "walls.json"_s));
    wall_atlas_array.shrink_to_fit();
    wall_atlas_map.clear();
    wall_atlas_map.reserve(wall_atlas_array.size()*2);

    for (auto& x : wall_atlas_array)
    {
        fm_soft_assert(x.name != "<invalid>"_s);
        fm_soft_assert(check_atlas_name(x.name));
        StringView name = x.name;
        wall_atlas_map[name] = &x;
        fm_debug_assert(name.data() == wall_atlas_map[name]->name.data());
    }

    fm_assert(!wall_atlas_map.empty());
}

ArrayView<const wall_info> loader_impl::wall_atlas_list()
{
    if (wall_atlas_map.empty()) [[unlikely]]
    {
        get_wall_atlas_list();
        fm_assert(!wall_atlas_map.empty());
    }
    return wall_atlas_array;
}

} // namespace floormat::loader_detail
