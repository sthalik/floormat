#include "impl.hpp"
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
} // namespace floormat

namespace floormat::loader_detail {

std::shared_ptr<ground_atlas> loader_impl::get_ground_atlas(StringView name, Vector2ub size, pass_mode pass)
{
    fm_assert(name != "<invalid>"_s);

    char buf[FILENAME_MAX];
    auto filename = make_atlas_path(buf, loader.GROUND_TILESET_PATH, name);
    auto tex = texture(""_s, filename);

    auto info = ground_info{name, {}, size, pass};
    auto atlas = std::make_shared<class ground_atlas>(info, filename, tex);
    return atlas;
}

// todo copypasta from wall-atlas.cpp
std::shared_ptr<class ground_atlas> loader_impl::ground_atlas(StringView name, bool fail_ok) noexcept(false)
{
    fm_soft_assert(check_atlas_name(name));
    auto it = ground_atlas_map.find(name);

    if (it != ground_atlas_map.end()) [[likely]]
    {
        if (it->second == (ground_info*)-1) [[unlikely]]
        {
           if (fail_ok) [[likely]]
                goto missing_ok;
            else
                goto error;
        }
        else if (!it->second->atlas)
            return it->second->atlas = get_ground_atlas(name, it->second->size, it->second->pass);
        else
            return it->second->atlas;
    }
    else
    {
        if (fail_ok) [[likely]]
            goto missing;
        else
            goto error;
    }

missing:
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
        get_ground_atlas_list();
    fm_assert(!ground_atlas_map.empty());
    return ground_atlas_array;
}

void loader_impl::get_ground_atlas_list()
{
    fm_assert(ground_atlas_map.empty());

    ground_atlas_array = json_helper::from_json<std::vector<ground_info>>(
        Path::join(loader_::GROUND_TILESET_PATH, "ground.json"_s));
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
        ground_info{loader.INVALID, {}, Vector2ub{1,1}, pass_mode::pass},
        ""_s, make_error_texture(Vector2ui(iTILE_SIZE2)));
    invalid_ground_atlas = Pointer<ground_info>{
        InPlaceInit, atlas->name(),
        atlas, atlas->num_tiles2(), atlas->pass_mode()};
    return *invalid_ground_atlas;
}

} // namespace floormat::loader_detail
