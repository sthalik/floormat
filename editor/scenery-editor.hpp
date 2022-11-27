#pragma once
#include "src/scenery.hpp"
#include <map>
#include <memory>
#include <Corrade/Containers/String.h>

namespace floormat {

struct anim_atlas;

struct scenery_editor final
{
    struct scenery_ {
        String name, descr;
        scenery_proto proto;
    };

    scenery_editor() noexcept;

    void set_rotation(enum rotation r);
    enum rotation rotation() const;
    void next_rotation();
    void prev_rotation();

    void select_tile(const scenery_& s);
    void clear_selection();
    const scenery_& get_selected();
    bool is_atlas_selected(const std::shared_ptr<anim_atlas>& atlas) const;
    bool is_item_selected(const scenery_& s) const;

private:
    void load_atlases();

    std::map<String, scenery_> _atlases;
    scenery_ _selected;
};

} // namespace floormat
