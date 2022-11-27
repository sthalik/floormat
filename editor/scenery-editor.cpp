#include "scenery-editor.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"

namespace floormat {

using rotation_ = rotation;
using rotation_t = std::underlying_type_t<rotation_>;

scenery_editor::scenery_editor() noexcept
{
    load_atlases();
}

void scenery_editor::set_rotation(rotation_ r)
{
    _selected.frame.r = r;
}

rotation_ scenery_editor::rotation() const
{
    return _selected.frame.r;
}

void scenery_editor::next_rotation()
{
    // todo
    auto r_1 = (rotation_t)_selected.frame.r + 1;
    auto rot = (rotation_)r_1;
    if (rot >= rotation_COUNT)
        rot = (rotation_)0;
    _selected.frame.r = rot;
}

void scenery_editor::prev_rotation()
{
    if (_selected.frame.r == (rotation_)0)
        _selected.frame.r = (rotation_)((rotation_t)rotation_COUNT - 1);
    else
        _selected.frame.r = (rotation_)((rotation_t)_selected.frame.r - 1);
}

//return atlas == _selected.atlas && r == _selected.s.r && frame == _selected.s.frame;

void scenery_editor::select_tile(const scenery_proto& proto)
{
    _selected = proto;
}

void scenery_editor::clear_selection()
{
    _selected = {};
}

const scenery_proto& scenery_editor::get_selected()
{
    return _selected;
}

bool scenery_editor::is_atlas_selected(const std::shared_ptr<anim_atlas>& atlas) const
{
    return _selected.atlas == atlas;
}

bool scenery_editor::is_item_selected(const scenery_proto& proto) const
{
    return _selected == proto;
}

void scenery_editor::load_atlases()
{
    _atlases.clear();
    for (StringView str : loader.anim_atlas_list())
        _atlases[str] = loader.anim_atlas(str);
}

} // namespace floormat
