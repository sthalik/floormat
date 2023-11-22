#include "loader/impl.hpp"
#include "wall-info.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "wall-info.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/corrade-string.hpp"
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>
#include <vector>

namespace floormat {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(wall_info, name, descr)
} // namespace floormat

namespace floormat::loader_detail {

std::shared_ptr<wall_atlas> loader_impl::get_wall_atlas(StringView filename)
{
    using namespace floormat::Wall;
    using namespace floormat::Wall::detail;

    const auto jroot = json_helper::from_json_(filename);
    auto header = read_info_header(jroot);
    auto frames = read_all_frames(jroot);

    size_t direction_count = 0;
    for (const auto& [str, curdir] : wall_atlas::directions)
        if (jroot.contains(std::string_view{str.data(), str.size()}))
            direction_count++;
    fm_soft_assert(direction_count > 0);
    fm_debug_assert(direction_count <= (size_t)Direction_::COUNT);

    auto directions = Array<Direction>{direction_count};
    std::array<DirArrayIndex, 4> dir_array_indexes{};

    uint8_t dir_idx = 0;
    for (const auto& [str, curdir] : wall_atlas::directions)
    {
        if (!jroot.contains(std::string_view{str.data(), str.size()}))
            continue;
        auto i = (size_t)curdir;
        fm_debug_assert(dir_idx < direction_count);
        dir_array_indexes[i] = { .val = dir_idx };
        directions[dir_idx++] = read_direction_metadata(jroot, curdir);
    }
    fm_debug_assert(dir_idx == direction_count);

    auto atlas = std::make_shared<class wall_atlas>();
    return atlas;
}

const wall_info& loader_impl::wall_atlas(StringView name, StringView dir)
{
    fm_soft_assert(check_atlas_name(name));
    char buf[FILENAME_MAX];
    auto path = make_atlas_path(buf, dir, name);

    auto it = wall_atlas_map.find(path);
    if (it == wall_atlas_map.end())
        fm_throw("no such wall atlas '{}'"_cf, fmt::string_view{path.data(), path.size()});
    fm_assert(it->second != nullptr);
    if (!it->second->atlas)
    {
        const_cast<wall_info*>(it->second)->atlas = get_wall_atlas(path);
    }
    return *it->second;
}

void loader_impl::get_wall_atlas_list()
{
    wall_atlas_array = json_helper::from_json<std::vector<wall_info>>(Path::join(WALL_TILESET_PATH, "walls.json"_s));
    wall_atlas_array.shrink_to_fit();
    wall_atlas_map.clear();
    wall_atlas_map.reserve(wall_atlas_array.size()*2);

    for (const auto& x : wall_atlas_array)
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
    return wall_atlas_array;
}

} // namespace floormat::loader_detail
