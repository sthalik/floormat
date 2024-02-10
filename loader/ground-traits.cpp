#include "ground-traits.hpp"
#include "atlas-loader-storage.hpp"
#include "ground-cell.hpp"
#include "loader.hpp"
#include "src/tile-defs.hpp"
#include "src/ground-atlas.hpp"
#include "compat/assert.hpp"
#include "compat/vector-wrapper.hpp"
#include <cr/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/Pointer.h>
#include <Magnum/ImageView.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

using ground_traits = atlas_loader_traits<ground_atlas>;
StringView ground_traits::loader_name() { return "ground_atlas"_s; }
auto ground_traits::atlas_of(const Cell& x) -> const std::shared_ptr<Atlas>& { return x.atlas; }
auto ground_traits::atlas_of(Cell& x) -> std::shared_ptr<Atlas>& { return x.atlas; }
StringView ground_traits::name_of(const Cell& x) { return x.name; }
StringView ground_traits::name_of(const Atlas& x) { return x.name(); }
String& ground_traits::name_of(Cell& x) { return x.name; }

void ground_traits::ensure_atlases_loaded(Storage& st)
{
    if (!st.name_map.empty()) [[likely]]
        return;

    st.cell_array = ground_cell::load_atlases_from_json().vec;
    st.name_map.max_load_factor(0.4f);
    st.name_map.reserve((st.cell_array.size()+1)*3/2);
    st.name_map[loader.INVALID] = -1uz;

    for (auto& c : st.cell_array)
        if (c.name.isSmall())
            c.name = String{AllocatedInit, c.name};

    for (auto i = 0uz; const auto& c : st.cell_array)
    {
        fm_soft_assert(c.name != loader.INVALID);
        fm_soft_assert(loader.check_atlas_name(c.name));
        fm_soft_assert(!c.atlas);
        st.name_map[c.name] = i++;
    }
}

auto ground_traits::make_invalid_atlas(Storage& s) -> const Cell& // todo! store it in cell_array
{
    if (!s.invalid_atlas) [[unlikely]]
    {
        auto atlas = std::make_shared<Atlas>(
            ground_def{loader.INVALID, Vector2ub{1,1}, pass_mode::pass},
            loader.make_error_texture(Vector2ui(tile_size_xy)));
        s.invalid_atlas = Pointer<ground_cell>{ InPlaceInit, atlas, atlas->name(), atlas->num_tiles2(), atlas->pass_mode() };
    }
    return *s.invalid_atlas;
}

auto ground_traits::make_atlas(StringView name, const Cell& c) -> std::shared_ptr<Atlas>
{
    auto def = ground_def{name, c.size, c.pass};
    auto tex = loader.texture(loader.GROUND_TILESET_PATH, name);
    auto atlas = std::make_shared<Atlas>(def, tex);
    return atlas;
}

auto ground_traits::make_cell(StringView) -> Optional<Cell> { return {}; }

} // namespace floormat::loader_detail
