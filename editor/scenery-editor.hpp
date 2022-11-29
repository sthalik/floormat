#pragma once
#include "scenery.hpp"
#include <map>
#include <memory>
#include <Corrade/Containers/String.h>

namespace floormat {

struct anim_atlas;
struct global_coords;
struct world;

struct scenery_editor final
{
    struct scenery_ final {
        String name, descr;
        scenery_proto proto;
        operator bool() const noexcept;
    };

    scenery_editor() noexcept;

    void set_rotation(enum rotation r);
    enum rotation rotation() const;
    void next_rotation();
    void prev_rotation();

    void select_tile(const scenery_& s);
    void clear_selection();
    const scenery_& get_selected() const;
    bool is_atlas_selected(const std::shared_ptr<anim_atlas>& atlas) const;
    bool is_item_selected(const scenery_& s) const;
    bool is_anything_selected() const;
    void place_tile(world& w, global_coords pos, const scenery_& s);

    auto cbegin() const noexcept { return _atlases.cbegin(); }
    auto cend() const noexcept { return _atlases.cend(); }
    auto begin() const noexcept { return _atlases.cbegin(); }
    auto end() const noexcept { return _atlases.cend(); }

private:
    void load_atlases();

    std::map<String, scenery_> _atlases;
    scenery_ _selected;
};

} // namespace floormat
