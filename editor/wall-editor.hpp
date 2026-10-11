#pragma once
#include "editor-enums.hpp"
#include "src/rotation.hpp"
#include "src/global-coords.hpp"
#include "loader/wall-cell.hpp"
#include <map>

namespace floormat {

class world;
class wall_atlas;

class wall_editor
{
    std::map<StringView, wall_cell> _atlases;
    bptr<wall_atlas> _selected_atlas;
    enum rotation _r = rotation::N;

    void load_atlases();

public:
    typename std::map<StringView, wall_cell>::const_iterator cbegin() const noexcept;
    typename std::map<StringView, wall_cell>::const_iterator cend() const noexcept;
    typename std::map<StringView, wall_cell>::const_iterator begin() const noexcept;
    typename std::map<StringView, wall_cell>::const_iterator end() const noexcept;

    wall_editor();
    StringView name() const;

    enum rotation rotation() const;
    void set_rotation(enum rotation r);
    void toggle_rotation();

    bptr<wall_atlas> get_selected() const;
    void select_atlas(const bptr<wall_atlas>& atlas);
    void clear_selection();
    bool is_atlas_selected(const bptr<wall_atlas>& atlas) const;
    bool is_anything_selected() const;

    void place_tile(world& w, global_coords coords, const bptr<wall_atlas>& atlas);
    editor_snap_mode check_snap(int mods) const;
};

} // namespace floormat
