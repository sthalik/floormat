#include "impl.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "src/emplacer.hpp"
#include "src/tile-atlas.hpp"
#include "src/anim-atlas.hpp"
#include <algorithm>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

std::shared_ptr<tile_atlas> loader_impl::tile_atlas(StringView name, Vector2ub size, Optional<pass_mode> pass) noexcept(false)
{
    fm_soft_assert(check_atlas_name(name));

    const emplacer e{[&] { return std::make_shared<struct tile_atlas>(name, texture(IMAGE_PATH, name), size, pass); }};
    auto atlas = tile_atlas_map.try_emplace(name, e).first->second;
    fm_soft_assert(!pass || pass == atlas->pass_mode());
    return atlas;
}

std::shared_ptr<struct tile_atlas> loader_impl::tile_atlas(StringView filename) noexcept(false)
{
    fm_assert(!tile_atlas_map.empty());
    auto it = tile_atlas_map.find(filename);
    if (it == tile_atlas_map.end())
        fm_throw("no such tile atlas '{}'"_cf, filename.data());
    return it->second;
}

ArrayView<String> loader_impl::anim_atlas_list()
{
    if (anim_atlases.empty())
        get_anim_atlas_list();
    return anim_atlases;
}

std::shared_ptr<anim_atlas> loader_impl::anim_atlas(StringView name, StringView dir) noexcept(false)
{
    fm_soft_assert(check_atlas_name(name));

    if (auto it = anim_atlas_map.find(name); it != anim_atlas_map.end())
        return it->second;
    else
    {
        const auto path = Path::join(dir, Path::splitExtension(name).first());
        auto anim_info = deserialize_anim(path + ".json");

        for (anim_group& group : anim_info.groups)
        {
            if (!group.mirror_from.isEmpty())
            {
                auto it = std::find_if(anim_info.groups.cbegin(), anim_info.groups.cend(),
                                       [&](const anim_group& x) { return x.name == group.mirror_from; });
                if (it == anim_info.groups.cend())
                    fm_throw("can't find group '{}' to mirror from '{}'"_cf, group.mirror_from.data(), group.name.data());
                group.frames = it->frames;
                for (anim_frame& f : group.frames)
                    f.ground = Vector2i((Int)f.size[0] - f.ground[0], f.ground[1]);
            }
        }

        auto tex = texture("", path);

        fm_soft_assert(!anim_info.object_name.isEmpty());
        fm_soft_assert(anim_info.pixel_size.product() > 0);
        fm_soft_assert(!anim_info.groups.empty());
        fm_soft_assert(anim_info.nframes > 0);
        fm_soft_assert(anim_info.nframes == 1 || anim_info.fps > 0);
        const auto size = tex.pixels().size();
        const auto width = size[1], height = size[0];
        fm_soft_assert(Vector2uz{anim_info.pixel_size} == Vector2uz{width, height});

        auto atlas = std::make_shared<struct anim_atlas>(path, tex, std::move(anim_info));
        return anim_atlas_map[atlas->name()] = atlas;
    }
}

void loader_impl::get_anim_atlas_list()
{
    anim_atlases.clear();
    using f = Path::ListFlag;
    constexpr auto flags = f::SkipDirectories | f::SkipDotAndDotDot | f::SkipSpecial | f::SortAscending;
    if (const auto list = Path::list(ANIM_PATH, flags); list)
    {
        anim_atlases.reserve(list->size()*2);
        for (StringView str : *list)
            if (str.hasSuffix(".json"))
                anim_atlases.emplace_back(str.exceptSuffix(std::size(".json")-1));
    }
}

bool loader_impl::check_atlas_name(StringView str)
{
    if (str.isEmpty())
        return false;
    if (str.findAny("\0\\<>&;:^'\""_s) || str.find("/."_s))
        return false;
    if (str[0] == '.' || str[0] == '/')
        return false;

    return true;
}

} // namespace floormat::loader_detail
