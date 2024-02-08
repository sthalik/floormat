#include "ground-traits.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "atlas-loader.hpp"
#include "atlas-loader-storage.hpp"
#include "ground-cell.hpp"
#include "loader.hpp"
#include "src/tile-defs.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/ground-atlas.hpp"
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

using traits = atlas_loader_traits<ground_atlas>;

StringView traits::loader_name() { return "ground_atlas"_s; }
auto traits::atlas_of(const Cell& x) -> const std::shared_ptr<Atlas>& { return x.atlas; }
auto traits::atlas_of(Cell& x) -> std::shared_ptr<Atlas>& { return x.atlas; }
StringView traits::name_of(const Cell& x) { return x.name; }
StringView traits::name_of(const Atlas& x) { return x.name(); }

void traits::ensure_atlases_loaded(Storage& st)
{
    if (!st.is_empty()) [[likely]]
        return;

    fm_assert(st.cell_array.empty());
    fm_assert(st.name_map.empty());

    auto defs = json_helper::from_json<std::vector<ground_def>>(Path::join(loader_::GROUND_TILESET_PATH, "ground.json"_s));
    std::vector<ground_cell> infos;
    infos.reserve(defs.size());

    for (auto& x : defs)
        infos.push_back(ground_cell{{}, std::move(x.name), x.size, x.pass});

    st.cell_array = Utility::move(infos);
    fm_assert(!st.cell_array.empty());
    fm_assert(st.name_map.empty());

    constexpr bool add_invalid = true;

    if constexpr(add_invalid)
        for (auto& x : st.cell_array)
            fm_soft_assert(x.name != loader.INVALID);

    if constexpr(true) // NOLINT(*-simplify-boolean-expr)
        st.cell_array.push_back(make_invalid_atlas(st)); // add invalid atlas

    st.name_map.reserve(st.cell_array.size());

    for (auto& x : st.cell_array)
    {
        if constexpr(!add_invalid)
            fm_soft_assert(x.name != loader.INVALID);
        fm_soft_assert(loader.check_atlas_name(x.name));
        st.name_map[x.name] = &x;
    }

    fm_assert(!st.cell_array.empty());
    fm_assert(!st.name_map.empty());
    fm_debug_assert(!st.is_empty());
}

auto traits::make_invalid_atlas(Storage& s) -> const Cell&
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

auto traits::make_atlas(StringView name, const Cell& cell) -> std::shared_ptr<Atlas>
{
    auto def = ground_def{name, cell.size, cell.pass};
    auto tex = loader.texture(loader.GROUND_TILESET_PATH, name);
    auto atlas = std::make_shared<Atlas>(def, tex);
    return atlas;
}

} // namespace floormat::loader_detail
