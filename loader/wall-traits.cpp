#include "wall-traits.hpp"
#include "atlas-loader-storage.hpp"
#include "wall-cell.hpp"
#include "loader.hpp"
#include "src/tile-defs.hpp"
#include "src/wall-atlas.hpp"
#include "compat/array-size.hpp"
#include "compat/exception.hpp"
#include <cr/StringView.h>
#include <cr/Optional.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>

namespace floormat::loader_detail {

using wall_traits = atlas_loader_traits<wall_atlas>;
StringView wall_traits::loader_name() { return "wall_atlas"_s; }
auto wall_traits::atlas_of(const Cell& x) -> const std::shared_ptr<Atlas>& { return x.atlas; }
auto wall_traits::atlas_of(Cell& x) -> std::shared_ptr<Atlas>& { return x.atlas; }
StringView wall_traits::name_of(const Cell& x) { return x.name; }
String& wall_traits::name_of(Cell& x) { return x.name; }

void wall_traits::atlas_list(Storage& s)
{
    fm_debug_assert(s.name_map.empty());
    s.cell_array = wall_cell::load_atlases_from_json();
    s.name_map[loader.INVALID] = -1uz;
}

auto wall_traits::make_invalid_atlas(Storage& s) -> Cell
{
    fm_debug_assert(!s.invalid_atlas);
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
            {true, false},
        }, name, loader.make_error_texture(frame_size));
    return { .atlas = move(a), .name = name, };
}

auto wall_traits::make_atlas(StringView name, const Cell&) -> std::shared_ptr<Atlas>
{
    char file_buf[fm_FILENAME_MAX], json_buf[fm_FILENAME_MAX];
    auto file = loader.make_atlas_path(file_buf, loader.WALL_TILESET_PATH, name);
    int json_size = std::snprintf(json_buf, array_size(json_buf), "%s.json", file_buf);
    fm_soft_assert(json_size != 0 && (size_t)json_size <= std::size_t(json_buf));
    auto json_name = StringView{json_buf, (size_t)json_size};
    auto def = wall_atlas_def::deserialize(json_name);
    fm_soft_assert(name == def.header.name);
    fm_soft_assert(!def.frames.isEmpty());
    auto tex = loader.texture(""_s, file);
    auto atlas = std::make_shared<class wall_atlas>(move(def), file, tex);
    return atlas;
}

auto wall_traits::make_cell(StringView) -> Optional<Cell> { return {}; }

} // namespace floormat::loader_detail
