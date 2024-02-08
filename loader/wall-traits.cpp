#include "wall-traits.hpp"
#include "compat/exception.hpp"
#include "compat/vector-wrapper.hpp"
#include "atlas-loader-storage.hpp"
#include "wall-cell.hpp"
#include "loader.hpp"
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
    fm_assert(st.name_map.empty());

    st.cell_array = wall_cell::load_atlases_from_json().vec;
    st.name_map.reserve(st.cell_array.size());

    for (auto& x : st.cell_array)
    {
        fm_soft_assert(x.name != "<invalid>"_s);
        fm_soft_assert(loader.check_atlas_name(x.name));
        StringView name = x.name;
        st.name_map[name] = &x;
    }

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
