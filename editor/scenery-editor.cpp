#include "scenery-editor.hpp"
#include "src/anim-atlas.hpp"
#include "src/loader.hpp"
#include "compat/assert.hpp"

namespace floormat {

using rotation_ = enum rotation;
using rotation_t = std::underlying_type_t<rotation_>;

scenery_editor::pair::operator bool() const { return atlas != nullptr; }

scenery_editor::scenery_editor() noexcept = default;

void scenery_editor::set_rotation(enum rotation r)
{
    _selected.r = r;
}

rotation scenery_editor::rotation() const
{
    return _selected.r;
}

void scenery_editor::next_rotation()
{
    auto r_1 = (rotation_t)_selected.r + 1;
    auto rot = (rotation_)r_1;
    if (rot >= rotation_::COUNT)
        rot = (rotation_)0;
    _selected.r = rot;
}

void scenery_editor::prev_rotation()
{
    if (_selected.r == (rotation_)0)
        _selected.r = (rotation_)((rotation_t)rotation_::COUNT - 1);
    else
        _selected.r = (rotation_)((rotation_t)_selected.r - 1);
}

//return atlas == _selected.atlas && r == _selected.s.r && frame == _selected.s.frame;

void scenery_editor::select_tile(const std::shared_ptr<anim_atlas>& atlas, rotation_ r, frame_t frame)
{
    fm_assert(frame < atlas->group(r).frames.size());
    _selected = { atlas, r, frame };
}

void scenery_editor::clear_selection()
{
    _selected = { nullptr, rotation::COUNT, scenery::NO_FRAME };
}

auto scenery_editor::get_selected() -> pair
{
    return _selected;
}

bool scenery_editor::is_atlas_selected(const std::shared_ptr<anim_atlas>& atlas) const
{
    return _selected.atlas == atlas;
}

bool scenery_editor::is_item_selected(const std::shared_ptr<anim_atlas>& atlas, enum rotation r, frame_t frame) const
{
    return is_atlas_selected(atlas) && _selected.r == r && _selected.frame == frame;
}

void scenery_editor::load_atlases()
{
    _atlases.clear();
    for (StringView str : loader.anim_atlas_list())
        _atlases[str] = loader.anim_atlas(str);
}

} // namespace floormat
