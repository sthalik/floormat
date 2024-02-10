#include "wall-traits.hpp"
#include "compat/exception.hpp"
#include "compat/vector-wrapper.hpp"
#include "atlas-loader-storage.hpp"
#include "wall-cell.hpp"
#include "loader.hpp"
#include "src/tile-defs.hpp"
#include "src/wall-atlas.hpp"
#include <cr/StringView.h>
#include <cr/Optional.h>
#include <cr/Pointer.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>

namespace floormat::loader_detail {

namespace { const auto placeholder_cell = wall_cell{}; }
using wall_traits = atlas_loader_traits<wall_atlas>;
StringView wall_traits::loader_name() { return "wall_atlas"_s; }
auto wall_traits::atlas_of(const Cell& x) -> const std::shared_ptr<Atlas>& { return x.atlas; }
auto wall_traits::atlas_of(Cell& x) -> std::shared_ptr<Atlas>& { return x.atlas; }
StringView wall_traits::name_of(const Cell& x) { return x.name; }
StringView wall_traits::name_of(const Atlas& x) { return x.name(); }
String& wall_traits::name_of(Cell& x) { return x.name; }

void wall_traits::ensure_atlases_loaded(Storage& st)
{
    if (!st.name_map.empty()) [[likely]]
        return;
    st.name_map.max_load_factor(0.4f);

    constexpr bool add_invalid = true;

    st.cell_array = wall_cell::load_atlases_from_json().vec;
    st.name_map.reserve(st.cell_array.size()*2);
    fm_assert(!st.cell_array.empty());
    fm_assert(st.name_map.empty());

    if constexpr(add_invalid)
    {
        for (auto& x : st.cell_array)
            fm_soft_assert(x.name != loader.INVALID);
        st.cell_array.push_back(make_invalid_atlas(st));
    }

    for (auto& c : st.cell_array)
    {
        if constexpr(!add_invalid)
            fm_soft_assert(c.name != "<invalid>"_s);
        fm_soft_assert(loader.check_atlas_name(c.name));
        StringView name = c.name;
        st.name_map[name] = &c;
    }

    fm_assert(!st.cell_array.empty());
}

auto wall_traits::make_invalid_atlas(Storage& st) -> const Cell&
{
    if (st.invalid_atlas) [[likely]]
        return *st.invalid_atlas;

    constexpr auto name = loader_::INVALID;
    constexpr auto frame_size = Vector2ui{tile_size_xy, tile_size_z};

    auto a = std::make_shared<class wall_atlas>(
        wall_atlas_def {
            Wall::Info{.name = name, .depth = 8},
            array<Wall::Frame>({{ {}, frame_size}, }),
            array<Wall::Direction>({{
                { .index = 0, .count = 1, .pixel_size = frame_size, .is_defined = true, }
            } }),
            {{ {.val = 0}, {}, }},
            {1u},
        }, name, loader.make_error_texture(frame_size));
    st.invalid_atlas = Pointer<wall_cell>{InPlaceInit, wall_cell{ .atlas = std::move(a), .name = name, } };
    return *st.invalid_atlas;
}

auto wall_traits::make_atlas(StringView name, const Cell&) -> std::shared_ptr<Atlas>
{
    char file_buf[fm_FILENAME_MAX], json_buf[fm_FILENAME_MAX];
    auto file = loader.make_atlas_path(file_buf, loader.WALL_TILESET_PATH, name);
    int json_size = std::snprintf(json_buf, std::size(json_buf), "%s.json", file_buf);
    fm_soft_assert(json_size != 0 && (size_t)json_size <= std::size_t(json_buf));
    auto json_name = StringView{json_buf, (size_t)json_size};
    auto def = wall_atlas_def::deserialize(json_name);
    auto tex = loader.texture(""_s, file);
    fm_soft_assert(name == def.header.name);
    fm_soft_assert(!def.frames.isEmpty());
    auto atlas = std::make_shared<class wall_atlas>(std::move(def), file, tex);
    return atlas;
}

auto wall_traits::make_cell(StringView) -> Optional<Cell> { return {}; }

} // namespace floormat::loader_detail
