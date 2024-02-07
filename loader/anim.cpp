#include "impl.hpp"
#include "loader/anim-cell.hpp"
#include "compat/exception.hpp"
#include "src/anim-atlas.hpp"
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat {

std::shared_ptr<class anim_atlas>
loader_::get_anim_atlas(StringView path) noexcept(false)
{
    auto anim_info = deserialize_anim_def(path + ".json");

    for (anim_group& group : anim_info.groups)
    {
        if (!group.mirror_from.isEmpty())
        {
            auto it = std::find_if(anim_info.groups.cbegin(), anim_info.groups.cend(),
                                   [&](const anim_group& x) { return x.name == group.mirror_from; });
            if (it == anim_info.groups.cend())
                fm_throw("can't find group '{}' to mirror from '{}'"_cf, group.mirror_from, group.name);
            group.frames = array(arrayView(it->frames));
            for (anim_frame& f : group.frames)
                f.ground = Vector2i((Int)f.size[0] - f.ground[0], f.ground[1]);
        }
    }

    auto tex = texture(""_s, path);

    fm_soft_assert(!anim_info.object_name.isEmpty());
    fm_soft_assert(anim_info.pixel_size.product() > 0);
    fm_soft_assert(!anim_info.groups.isEmpty());
    fm_soft_assert(anim_info.nframes > 0);
    fm_soft_assert(anim_info.nframes == 1 || anim_info.fps > 0);
    const auto size = tex.pixels().size();
    const auto width = size[1], height = size[0];
    fm_soft_assert(anim_info.pixel_size[0] == width && anim_info.pixel_size[1] == height);

    auto atlas = std::make_shared<class anim_atlas>(path, tex, std::move(anim_info));
    return atlas;
}

} // namespace floormat

namespace floormat::loader_detail {

ArrayView<const String> loader_impl::anim_atlas_list()
{
    if (anim_atlases.empty())
        get_anim_atlas_list();
    fm_assert(!anim_atlases.empty());
    return { anim_atlases.data(), anim_atlases.size() };
}

std::shared_ptr<anim_atlas> loader_impl::anim_atlas(StringView name, StringView dir, loader_policy policy) noexcept(false)
{
    if (name == INVALID) return make_invalid_anim_atlas().atlas; // todo! hack

    fm_soft_assert(check_atlas_name(name));
    fm_soft_assert(!dir || dir[dir.size()-1] == '/');
    char buf[fm_FILENAME_MAX];
    auto path = make_atlas_path(buf, dir, name);

    if (auto it = anim_atlas_map.find(path); it != anim_atlas_map.end())
        return it->second;
    else
    {
        auto atlas = get_anim_atlas(path);
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
        constexpr auto suffix = ".json"_s;
        for (StringView str : *list)
            if (str.hasSuffix(suffix))
                anim_atlases.emplace_back(str.exceptSuffix(suffix.size()));
    }
    anim_atlases.shrink_to_fit();
    fm_assert(!anim_atlases.empty());
}

const anim_cell& loader_impl::make_invalid_anim_atlas()
{
    if (invalid_anim_atlas) [[likely]]
        return *invalid_anim_atlas;

    constexpr auto size = Vector2ui{16};

    auto frame = anim_frame {
        .ground = Vector2i(size/2),
        .offset = {},
        .size = size,
    };
    auto groups = Array<anim_group>{ValueInit, 1};
    groups[0] = anim_group {
        .name = "n"_s,
        .frames = array({ frame }),
    };
    auto def = anim_def {
        .object_name = INVALID,
        .anim_name = INVALID,
        .groups = Utility::move(groups),
        .pixel_size = size,
        .scale = anim_scale::fixed{size.x(), true},
        .nframes = 1,
    };
    auto atlas = std::make_shared<class anim_atlas>(INVALID, make_error_texture(size), std::move(def));
    auto info = anim_cell{
        .name = INVALID,
        .atlas = atlas,
    };
    invalid_anim_atlas = Pointer<anim_cell>{ InPlace, std::move(info) };
    return *invalid_anim_atlas;
}

} // namespace floormat::loader_detail
