#include "scenery-traits.hpp"
#include "compat/assert.hpp"
#include "compat/vector-wrapper.hpp"
#include "atlas-loader-storage.hpp"
#include "scenery-cell.hpp"
#include "loader.hpp"
#include "anim-cell.hpp"
#include "src/tile-defs.hpp"
#include "src/anim-atlas.hpp"
#include "src/scenery.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "serialize/scenery.hpp"
#include "serialize/json-wrapper.hpp"
#include <cr/Optional.h>
#include <Corrade/Containers/StringView.h>

namespace floormat::loader_detail {

using scenery_traits = atlas_loader_traits<scenery_proto>;
StringView scenery_traits::loader_name() { return "scenery_proto"_s; }
auto scenery_traits::atlas_of(const Cell& x) -> const Optional<Atlas>& { return x.proto; }
auto scenery_traits::atlas_of(Cell& x) -> Optional<Atlas>& { return x.proto; }
StringView scenery_traits::name_of(const Cell& x) { return x.name; }
String& scenery_traits::name_of(Cell& x) { return x.name; }

void scenery_traits::atlas_list(Storage& s)
{
    fm_debug_assert(s.name_map.empty());
    s.cell_array = scenery_cell::load_atlases_from_json().vec;
    s.name_map[loader.INVALID] = -1uz;
}

auto scenery_traits::make_invalid_atlas(Storage& s) -> Cell
{
    fm_debug_assert(!s.invalid_atlas);
    auto proto = scenery_proto{};
    proto.atlas = loader.invalid_anim_atlas().atlas;
    proto.bbox_size = Vector2ub{tile_size_xy/2};
    proto.subtype = generic_scenery_proto{false, true};
    return { .name = loader.INVALID, .proto = proto, };
}

auto scenery_traits::make_atlas(StringView, const Cell& c) -> Optional<scenery_proto>
{
    scenery_proto p = c.data->j;
    fm_debug_assert(p.atlas);
    return p;
}

auto scenery_traits::make_cell(StringView) -> Optional<Cell>
{
    return {};
}

} // namespace floormat::loader_detail
