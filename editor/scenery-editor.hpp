#pragma once
#include "src/scenery.hpp"
#include <map>
#include <memory>
#include <Corrade/Containers/StringView.h>

namespace floormat {

struct anim_atlas;

struct scenery_editor final
{
    using frame_t = scenery::frame_t;

    scenery_editor() noexcept;

    void set_rotation(enum rotation r);
    enum rotation rotation() const;
    void next_rotation();
    void prev_rotation();

    void select_tile(const scenery_proto& proto);
    void clear_selection();
    const scenery_proto& get_selected();
    bool is_atlas_selected(const std::shared_ptr<anim_atlas>& atlas) const;
    bool is_item_selected(const scenery_proto& proto) const;

private:
    void load_atlases();

    std::map<StringView, std::shared_ptr<anim_atlas>> _atlases;
    scenery_proto _selected;
};

} // namespace floormat
