#include "impl.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "src/emplacer.hpp"
#include "src/tile-atlas.hpp"
#include "src/anim-atlas.hpp"
#include <cstdio>
#include <algorithm>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat {

StringView loader_::make_atlas_path(char(&buf)[FILENAME_MAX], StringView dir, StringView name)
{
    fm_soft_assert(!dir || dir[dir.size()-1] == '/');
    const auto dirsiz = dir.size(), namesiz = name.size();
    fm_soft_assert(dirsiz + namesiz + 1 < FILENAME_MAX);
    std::memcpy(buf, dir.data(), dirsiz);
    std::memcpy(&buf[dirsiz], name.data(), namesiz);
    auto len = dirsiz + namesiz;
    buf[len] = '\0';
    return StringView{buf, len, StringViewFlag::NullTerminated};
}

bool loader_::check_atlas_name(StringView str) noexcept
{
    constexpr auto first_char =
        "@_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"_s;
    if (str == "<invalid>"_s)
        return true;
    if (!str || !first_char.find(str[0]))
        return false;
    if (str.findAny("\\\"'\n\r\t\a\033\0|$!%{}^*?<>&;:^"_s) || str.find("/."_s) || str.find("//"_s))
        return false;

    return true;
}

} // namespace floormat

namespace floormat::loader_detail {

std::shared_ptr<tile_atlas> loader_impl::tile_atlas(StringView name, Vector2ub size, pass_mode pass) noexcept(false)
{
    if (auto it = tile_atlas_map.find(name); it != tile_atlas_map.end())
    {
        fm_assert(it->second->pass_mode() == pass);
        return it->second;
    }

    fm_soft_assert(check_atlas_name(name));

    char buf[FILENAME_MAX];
    auto path = make_atlas_path(buf, IMAGE_PATH, name);

    auto atlas = std::make_shared<class tile_atlas>(path, name, texture(""_s, path), size, pass);
    tile_atlas_map[atlas->name()] = atlas;
    return atlas;
}

std::shared_ptr<class tile_atlas> loader_impl::tile_atlas(StringView filename) noexcept(false)
{
    fm_assert(!tile_atlas_map.empty());
    auto it = tile_atlas_map.find(filename);
    if (it == tile_atlas_map.end())
        fm_throw("no such tile atlas '{}'"_cf, filename);
    return it->second;
}

ArrayView<const String> loader_impl::anim_atlas_list()
{
    if (anim_atlases.empty())
        get_anim_atlas_list();
    fm_assert(!anim_atlases.empty());
    return anim_atlases;
}

std::shared_ptr<anim_atlas> loader_impl::anim_atlas(StringView name, StringView dir) noexcept(false)
{
    fm_soft_assert(dir && dir[dir.size()-1] == '/');
    char buf[FILENAME_MAX];
    auto path = make_atlas_path(buf, dir, name);

    if (auto it = anim_atlas_map.find(path); it != anim_atlas_map.end())
        return it->second;
    else
    {
        fm_soft_assert(check_atlas_name(name));
        auto anim_info = deserialize_anim(path + ".json");

        for (anim_group& group : anim_info.groups)
        {
            if (!group.mirror_from.isEmpty())
            {
                auto it = std::find_if(anim_info.groups.cbegin(), anim_info.groups.cend(),
                                       [&](const anim_group& x) { return x.name == group.mirror_from; });
                if (it == anim_info.groups.cend())
                    fm_throw("can't find group '{}' to mirror from '{}'"_cf, group.mirror_from, group.name);
                group.frames = it->frames;
                for (anim_frame& f : group.frames)
                    f.ground = Vector2i((Int)f.size[0] - f.ground[0], f.ground[1]);
            }
        }

        auto tex = texture(""_s, path);

        fm_soft_assert(!anim_info.object_name.isEmpty());
        fm_soft_assert(anim_info.pixel_size.product() > 0);
        fm_soft_assert(!anim_info.groups.empty());
        fm_soft_assert(anim_info.nframes > 0);
        fm_soft_assert(anim_info.nframes == 1 || anim_info.fps > 0);
        const auto size = tex.pixels().size();
        const auto width = size[1], height = size[0];
        fm_soft_assert(anim_info.pixel_size[0] == width && anim_info.pixel_size[1] == height);

        auto atlas = std::make_shared<class anim_atlas>(path, tex, std::move(anim_info));
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
        anim_atlases.reserve(list->size());
        for (StringView str : *list)
            if (str.hasSuffix(".json"))
                anim_atlases.emplace_back(str.exceptSuffix(std::size(".json")-1));
    }
    anim_atlases.shrink_to_fit();
    fm_assert(!anim_atlases.empty());
}

} // namespace floormat::loader_detail
