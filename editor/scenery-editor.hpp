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

    struct pair final {
        std::shared_ptr<anim_atlas> atlas;
        enum rotation r = rotation_COUNT;
        frame_t frame = scenery::NO_FRAME;
        operator bool() const;
    };

    scenery_editor() noexcept;

    void set_rotation(enum rotation r);
    enum rotation rotation() const;
    void next_rotation();
    void prev_rotation();

    void select_tile(const std::shared_ptr<anim_atlas>& atlas, enum rotation r, frame_t frame);
    void clear_selection();
    pair get_selected();
    bool is_atlas_selected(const std::shared_ptr<anim_atlas>& atlas) const;
    bool is_item_selected(const std::shared_ptr<anim_atlas>& atlas, enum rotation r, frame_t frame) const;

private:
    void load_atlases();

    std::map<StringView, std::shared_ptr<anim_atlas>> _atlases;
    pair _selected;
};

} // namespace floormat
