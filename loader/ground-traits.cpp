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

void ground_traits::ensure_atlases_loaded(Storage& s)
{
    fm_debug_assert(s.name_map.empty());
    s.cell_array = ground_cell::load_atlases_from_json().vec;
    s.name_map[loader.INVALID] = -1uz;
}

auto ground_traits::make_invalid_atlas(Storage& s) -> Pointer<Cell>
{
    fm_debug_assert(!s.invalid_atlas);
    auto atlas = std::make_shared<Atlas>(
        ground_def{loader.INVALID, Vector2ub{1,1}, pass_mode::pass},
        loader.make_error_texture(Vector2ui(tile_size_xy)));
    return Pointer<ground_cell>{ InPlaceInit, atlas, atlas->name(), atlas->num_tiles2(), atlas->pass_mode() };
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
