#pragma once
#include "editor-enums.hpp"
#include "src/rotation.hpp"
#include "src/global-coords.hpp"
#include "loader/wall-cell.hpp"
#include <map>

namespace floormat {

class world;
class wall_atlas;

class wall_editor final
{
    std::map<StringView, wall_cell> _atlases;
    bptr<wall_atlas> _selected_atlas;
    enum rotation _r = rotation::N;

    void load_atlases();

public:
    const std::map<StringView, wall_cell>& items() const noexcept;

    wall_editor();
    StringView name() const;

    enum rotation rotation() const;
    void set_rotation(enum rotation r);
    void toggle_rotation();

    bptr<wall_atlas> get_selected() const; // todo not thread-safe, copies a non-atomic bptr
    void select_item(const wall_cell& cell);
    void clear_selection();
    bool is_item_selected(const wall_cell& cell) const;
    bool is_anything_selected() const;

    void place_wall(world& w, global_coords coords, const bptr<wall_atlas>& atlas);
    editor_snap_mode check_snap(int mods) const;
};

} // namespace floormat
