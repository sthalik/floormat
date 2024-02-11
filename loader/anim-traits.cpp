#include "anim-traits.hpp"
#include "atlas-loader-storage.hpp"
#include "anim-cell.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"
#include "loader.hpp"
#include "compat/exception.hpp"
#include <cr/StringView.h>
#include <cr/Pointer.h>
#include <cr/Optional.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>

namespace floormat::loader_detail {

using anim_traits = atlas_loader_traits<anim_atlas>;
StringView anim_traits::loader_name() { return "anim_atlas"_s; }
auto anim_traits::atlas_of(const Cell& x) -> const std::shared_ptr<Atlas>& { return x.atlas; }
auto anim_traits::atlas_of(Cell& x) -> std::shared_ptr<Atlas>& { return x.atlas; }
StringView anim_traits::name_of(const Cell& x) { return x.name; }
StringView anim_traits::name_of(const Atlas& x) { return x.name(); }
String& anim_traits::name_of(Cell& x) { return x.name; }

void anim_traits::ensure_atlases_loaded(Storage& s)
{
    fm_assert(s.name_map.empty());
    s.cell_array = {};
    s.cell_array.reserve(16);
    s.name_map[loader.INVALID] = -1uz;
}

auto anim_traits::make_invalid_atlas(Storage& s) -> Pointer<Cell>
{
    fm_debug_assert(!s.invalid_atlas);
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
        .object_name = loader.INVALID,
        .anim_name = loader.INVALID,
        .groups = Utility::move(groups),
        .pixel_size = size,
        .scale = anim_scale::fixed{size.x(), true},
        .nframes = 1,
    };
    auto atlas = std::make_shared<class anim_atlas>(loader.INVALID, loader.make_error_texture(size), std::move(def));
    auto info = anim_cell{
        .name = loader.INVALID,
        .atlas = atlas,
    };
    return Pointer<anim_cell>{ InPlace, std::move(info) };
}

auto anim_traits::make_atlas(StringView name, const Cell&) -> std::shared_ptr<Atlas>
{
    return {}; // todo
}

auto anim_traits::make_cell(StringView) -> Optional<Cell>
{
    return {};
}

} // namespace floormat::loader_detail
