#include "impl.hpp"
#include "compat/assert.hpp"
#include "src/emplacer.hpp"
#include "src/tile-atlas.hpp"
#include "src/anim-atlas.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

std::shared_ptr<tile_atlas> loader_impl::tile_atlas(StringView name, Vector2ub size)
{
    const emplacer e{[&] { return std::make_shared<struct tile_atlas>(name, texture(IMAGE_PATH, name), size); }};
    auto atlas = tile_atlas_map.try_emplace(name, e).first->second;
    return atlas;
}

ArrayView<String> loader_impl::anim_atlas_list()
{
    if (anim_atlases.empty())
        get_anim_atlas_list();
    return anim_atlases;
}

std::shared_ptr<anim_atlas> loader_impl::anim_atlas(StringView name)
{
    if (auto it = anim_atlas_map.find(name); it != anim_atlas_map.end())
        return it->second;
    else
    {
        const auto path = Path::join(ANIM_PATH, Path::splitExtension(name).first());
        auto anim_info = deserialize_anim(path + ".json");
        auto tex = texture("", path);

        fm_assert(!anim_info.anim_name.isEmpty() && !anim_info.object_name.isEmpty());
        fm_assert(anim_info.pixel_size.product() > 0);
        fm_assert(!anim_info.groups.empty());
        fm_assert(anim_info.nframes > 0);
        fm_assert(anim_info.nframes == 1 || anim_info.fps > 0);
        const auto size = tex.pixels().size();
        const auto width = size[1], height = size[0];
        fm_assert(Vector2uz{anim_info.pixel_size} == Vector2uz(width, height));

        auto atlas = std::make_shared<struct anim_atlas>(path, tex, std::move(anim_info));
        return anim_atlas_map[atlas->name()] = atlas;
    }
}

void loader_impl::get_anim_atlas_list()
{
    anim_atlases.clear();
    anim_atlases.reserve(64);
    using f = Path::ListFlag;
    constexpr auto flags = f::SkipDirectories | f::SkipDotAndDotDot | f::SkipSpecial | f::SortAscending;
    if (const auto list = Path::list(ANIM_PATH, flags); list)
        for (StringView str : *list)
            if (str.hasSuffix(".json"))
                anim_atlases.emplace_back(str.exceptSuffix(std::size(".json")-1));
}

} // namespace floormat::loader_detail
