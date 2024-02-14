#pragma once
#include "compat/safe-ptr.hpp"
#include "editor-enums.hpp"
#include "src/tile-image.hpp"
#include "src/global-coords.hpp"
#include <map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>

namespace floormat {

class world;
struct ground_cell;

class ground_editor final
{
    enum selection_mode : unsigned char { sel_none, sel_tile, sel_perm, };
    struct tuple;

    std::map<StringView, ground_cell> _atlases;
    tile_image_proto _selected_tile;
    safe_ptr<tuple> _permutation;
    selection_mode _selection_mode = sel_none;

    void load_atlases();
    tile_image_proto get_selected_perm();

public:
    ground_editor();
    ~ground_editor() noexcept;
    std::shared_ptr<ground_atlas> maybe_atlas(StringView str);
    std::shared_ptr<ground_atlas> atlas(StringView str);
    typename std::map<StringView, ground_cell>::const_iterator begin() const noexcept;
    typename std::map<StringView, ground_cell>::const_iterator end() const noexcept;
    StringView name() const noexcept;

    void clear_selection();
    void select_tile(const std::shared_ptr<ground_atlas>& atlas, size_t variant);
    void select_tile_permutation(const std::shared_ptr<ground_atlas>& atlas);
    bool is_tile_selected(const std::shared_ptr<const ground_atlas>& atlas, size_t variant) const;
    bool is_permutation_selected(const std::shared_ptr<const ground_atlas>& atlas) const;
    bool is_atlas_selected(const std::shared_ptr<const ground_atlas>& atlas) const;
    bool is_anything_selected() const;
    tile_image_proto get_selected();
    void place_tile(world& world, global_coords pos, const tile_image_proto& img);
    editor_snap_mode check_snap(int mods) const;
};

} // namespace floormat
