#include "ground-traits.hpp"
#include "atlas-loader-storage.hpp"
#include "ground-cell.hpp"
#include "loader.hpp"
#include "src/tile-defs.hpp"
#include "src/ground-atlas.hpp"
#include "compat/assert.hpp"
#include <cr/Optional.h>
#include <mg/ImageView.h>
#include <mg/ImageData.h>

namespace floormat::detail::loader {

using ground_traits = atlas_loader_traits<ground_atlas>;
StringView ground_traits::loader_name() { return "ground_atlas"_s; }
auto ground_traits::atlas_of(const Cell& x) -> const ::floormat::bptr<Atlas>& { return x.atlas; }
auto ground_traits::atlas_of(Cell& x) -> ::floormat::bptr<Atlas>& { return x.atlas; }
StringView ground_traits::name_of(const Cell& x) { return x.name; }
String& ground_traits::name_of(Cell& x) { return x.name; }

void ground_traits::atlas_list(Storage& s)
{
    fm_debug_assert(s.name_map.empty());
    s.cell_array = ground_cell::load_atlases_from_json();
    s.name_map[::floormat::loader.INVALID] = -1uz;
}

auto ground_traits::make_invalid_atlas(Storage& s) -> Cell
{
    fm_debug_assert(!s.invalid_atlas);
    auto atlas = ::floormat::bptr<Atlas>{InPlace,
        ground_def{::floormat::loader.INVALID, Vector2ub{1,1}, pass_mode::pass},
        ::floormat::loader.make_error_texture(Vector2ui(tile_size_xy))};
    return ground_cell{ atlas, atlas->name(), atlas->num_tiles2(), atlas->pass_mode() };
}

auto ground_traits::make_atlas(StringView name, const Cell& c) -> ::floormat::bptr<Atlas>
{
    auto def = ground_def{name, c.size, c.pass};
    auto tex = ::floormat::loader.texture(::floormat::loader.GROUND_TILESET_PATH, name);
    auto atlas = ::floormat::bptr<Atlas>{InPlace, def, tex};
    return atlas;
}

auto ground_traits::make_cell(StringView) -> Optional<Cell> { return {}; }

} // namespace floormat::detail::loader
