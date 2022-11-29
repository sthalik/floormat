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
    if (_selected.proto.atlas)
    {
        (void)_selected.proto.atlas->group(r);
        _selected.proto.frame.r = r;
    }
}

rotation_ scenery_editor::rotation() const
{
    return _selected.proto.frame.r;
}

void scenery_editor::next_rotation()
{
    if (auto& proto = _selected.proto; proto.atlas)
        proto.frame.r = proto.atlas->next_rotation_from(proto.frame.r);
}

void scenery_editor::prev_rotation()
{
    if (auto& proto = _selected.proto; proto.atlas)
        proto.frame.r = proto.atlas->prev_rotation_from(proto.frame.r);
}

void scenery_editor::select_tile(const scenery_& s)
{
    const auto rot = is_anything_selected() && s.proto.atlas->check_rotation(_selected.proto.frame.r)
                     ? _selected.proto.frame.r
                     : s.proto.frame.r;
    _selected = s;
    _selected.proto.frame.r = rot;
}

void scenery_editor::clear_selection()
{
    _selected = {};
}

auto scenery_editor::get_selected() -> const scenery_&
{
    return _selected;
}

bool scenery_editor::is_atlas_selected(const std::shared_ptr<anim_atlas>& atlas) const
{
    return atlas == _selected.proto.atlas;
}

bool scenery_editor::is_item_selected(const scenery_& s) const
{
    return s.name == _selected.name && s.proto.atlas == _selected.proto.atlas;
}

bool scenery_editor::is_anything_selected() const
{
    return _selected.proto.atlas != nullptr;
}

} // namespace floormat
