#pragma once

#include "editor-enums.hpp"
#include "src/tile-image.hpp"
#include "global-coords.hpp"
#include <memory>
#include <vector>
#include <map>
#include <Corrade/Containers/String.h>

namespace floormat {

struct world;

struct tile_editor final
{
private:
    enum selection_mode : std::uint8_t {
        sel_none, sel_tile, sel_perm,
    };

    struct tuple final {
        std::shared_ptr<tile_atlas> atlas;
        std::vector<decltype(tile_image_proto::variant)> variant;
    };

    String _name;
    std::map<std::string, std::shared_ptr<tile_atlas>> _atlases;
    tile_image_proto _selected_tile;
    tuple _permutation;
    selection_mode _selection_mode = sel_none;
    editor_mode _mode;
    editor_wall_rotation _rotation = editor_wall_rotation::N;

    void load_atlases();
    tile_image_proto get_selected_perm();

public:
    tile_editor(editor_mode mode, StringView name);
    std::shared_ptr<tile_atlas> maybe_atlas(StringView str);
    std::shared_ptr<tile_atlas> atlas(StringView str);
    auto cbegin() const noexcept { return _atlases.cbegin(); }
    auto cend() const noexcept { return _atlases.cend(); }
    auto begin() const noexcept { return _atlases.cbegin(); }
    auto end() const noexcept { return _atlases.cend(); }
    StringView name() const noexcept { return _name; }
    editor_mode mode() const noexcept { return _mode; }
    editor_wall_rotation rotation() const noexcept { return _rotation; }

    void clear_selection();
    void select_tile(const std::shared_ptr<tile_atlas>& atlas, std::size_t variant);
    void select_tile_permutation(const std::shared_ptr<tile_atlas>& atlas);
    bool is_tile_selected(const std::shared_ptr<const tile_atlas>& atlas, std::size_t variant) const;
    bool is_permutation_selected(const std::shared_ptr<const tile_atlas>& atlas) const;
    bool is_atlas_selected(const std::shared_ptr<const tile_atlas>& atlas) const;
    tile_image_proto get_selected();
    void place_tile(world& world, global_coords pos, const tile_image_proto& img);
    void toggle_rotation();
    void set_rotation(editor_wall_rotation r);
    editor_snap_mode check_snap(int mods) const;
    bool can_rotate() const;
};

} // namespace floormat
