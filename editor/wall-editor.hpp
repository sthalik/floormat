#pragma once
#include "editor-enums.hpp"
#include "src/rotation.hpp"
#include "src/global-coords.hpp"
#include <memory>
#include <map>

namespace floormat {

struct world;
class wall_atlas;

class wall_editor
{
    std::map<StringView, std::shared_ptr<wall_atlas>> _atlases;
    std::shared_ptr<wall_atlas> _selected_atlas;
    rotation _r = rotation::N;

    void load_atlases();

public:
    wall_editor();
    StringView name() const;

    enum rotation rotation() const;
    void set_rotation(enum rotation r);
    void toggle_rotation();

    const wall_atlas* get_selected() const;
    void select_atlas(const std::shared_ptr<wall_atlas>& atlas);
    void clear_selection();
    bool is_atlas_selected(const std::shared_ptr<wall_atlas>& atlas) const;
    bool is_anything_selected() const;

    void place_tile(world& w, global_coords coords, const std::shared_ptr<wall_atlas>& atlas);
    editor_snap_mode check_snap(int mods) const;
};

} // namespace floormat
