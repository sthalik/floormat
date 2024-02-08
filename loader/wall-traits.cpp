#include "wall-traits.hpp"
#include "compat/assert.hpp"
#include "atlas-loader-storage.hpp"
#include "wall-cell.hpp"
#include "src/wall-atlas.hpp"
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/Pointer.h>

namespace floormat::loader_detail {

StringView atlas_loader_traits<wall_atlas>::loader_name() { return "wall_atlas"_s; }
auto atlas_loader_traits<wall_atlas>::atlas_of(const Cell& x) -> const std::shared_ptr<Atlas>& { return x.atlas; }
auto atlas_loader_traits<wall_atlas>::atlas_of(Cell& x) -> std::shared_ptr<Atlas>& { return x.atlas; }
StringView atlas_loader_traits<wall_atlas>::name_of(const Cell& x) { return x.name; }
StringView atlas_loader_traits<wall_atlas>::name_of(const Atlas& x) { return x.name(); }

using traits = atlas_loader_traits<wall_atlas>;

void traits::ensure_atlases_loaded(Storage& st)
{
    if (!st.cell_array.empty()) [[likely]]
        return;

#if 0
    wall_atlas_array = json_helper::from_json<std::vector<wall_cell>>(Path::join(WALL_TILESET_PATH, "walls.json"_s));
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
#endif

    fm_assert("todo" && false);

    fm_assert(!st.cell_array.empty());
}

auto traits::make_invalid_atlas(Storage& st) -> const Cell&
{
    fm_assert("todo" && false);
}

auto traits::make_atlas(StringView name, const Cell& c) -> std::shared_ptr<Atlas>
{
    fm_assert("todo" && false);
}

} // namespace floormat::loader_detail
