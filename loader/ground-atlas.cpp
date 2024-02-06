#include "impl.hpp"
#include "src/tile-constants.hpp"
#include "src/ground-atlas.hpp"
#include "compat/exception.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/ground-atlas.hpp"
#include "src/tile-defs.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>

namespace floormat {
using loader_detail::loader_impl;

std::shared_ptr<ground_atlas>
loader_::get_ground_atlas(StringView name, Vector2ub size, pass_mode pass) noexcept(false)
{
    fm_assert(name != "<invalid>"_s);

    char buf[FILENAME_MAX];
    auto filename = make_atlas_path(buf, loader.GROUND_TILESET_PATH, name);
    auto tex = texture(""_s, filename);

    auto info = ground_def{name, size, pass};
    auto atlas = std::make_shared<class ground_atlas>(info, filename, tex);
    return atlas;
}

} // namespace floormat

namespace floormat::loader_detail {

// todo copypasta from wall-atlas.cpp
std::shared_ptr<class ground_atlas> loader_impl::ground_atlas(StringView name, loader_policy policy) noexcept(false)
{
    (void)ground_atlas_list();

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
    auto it = ground_atlas_map.find(name);

    if (it != ground_atlas_map.end()) [[likely]]
    {
        if (it->second == (ground_info*)-1) [[unlikely]]
        {
            switch (policy)
            {
            case loader_policy::error:
                goto error;
            case loader_policy::warn:
            case loader_policy::ignore:
                goto missing_ok;
            }
            fm_assert(false);
            std::unreachable();
        }
        else if (!it->second->atlas)
            return it->second->atlas = get_ground_atlas(name, it->second->size, it->second->pass);
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
        fm_assert(false);
        std::unreachable();
    }

missing_warn:
    {
        missing_ground_atlases.push_back(String { AllocatedInit, name });
        auto string_view = StringView{missing_ground_atlases.back()};
        ground_atlas_map[string_view] = (ground_info*)-1;
    }

    if (name != "<invalid>")
        DBG_nospace << "ground_atlas '" << name << "' doesn't exist";

missing_ok:
    return make_invalid_ground_atlas().atlas;

error:
    fm_throw("no such ground atlas '{}'"_cf, name);
}

ArrayView<const ground_info> loader_impl::ground_atlas_list() noexcept(false)
{
    if (ground_atlas_map.empty()) [[unlikely]]
    {
        get_ground_atlas_list();
        fm_assert(!ground_atlas_map.empty());
    }
    return ground_atlas_array;
}

void loader_impl::get_ground_atlas_list()
{
    fm_assert(ground_atlas_map.empty());

    auto defs = json_helper::from_json<std::vector<ground_def>>(Path::join(loader_::GROUND_TILESET_PATH, "ground.json"_s));
    std::vector<ground_info> infos;
    infos.reserve(defs.size());

    for (auto& x : defs)
        infos.push_back(ground_info{std::move(x.name), {}, x.size, x.pass});

    ground_atlas_array = infos;
    ground_atlas_array.shrink_to_fit();
    ground_atlas_map.clear();
    ground_atlas_map.reserve(ground_atlas_array.size()*2);

    for (auto& x : ground_atlas_array)
    {
        fm_soft_assert(x.name != "<invalid>"_s);
        fm_soft_assert(check_atlas_name(x.name));
        StringView name = x.name;
        ground_atlas_map[name] = &x;
        fm_debug_assert(name.data() == ground_atlas_map[name]->name.data());
    }

    fm_assert(!ground_atlas_map.empty());
}

const ground_info& loader_impl::make_invalid_ground_atlas()
{
    if (invalid_ground_atlas) [[likely]]
        return *invalid_ground_atlas;

    auto atlas = std::make_shared<class ground_atlas>(
        ground_def{loader.INVALID, Vector2ub{1,1}, pass_mode::pass},
        ""_s, make_error_texture(Vector2ui(iTILE_SIZE2)));
    invalid_ground_atlas = Pointer<ground_info>{
        InPlaceInit, atlas->name(),
        atlas, atlas->num_tiles2(), atlas->pass_mode()};
    return *invalid_ground_atlas;
}

} // namespace floormat::loader_detail
