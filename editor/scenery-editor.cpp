#include "scenery-editor.hpp"
#include "src/anim-atlas.hpp"

namespace floormat {

using rotation_ = enum rotation;
using rotation_t = std::underlying_type_t<rotation_>;

scenery_editor::scenery_editor() noexcept
{
}

void scenery_editor::set_rotation(enum rotation r)
{
    _selected.s.r = r;
}

rotation scenery_editor::rotation() const
{
    return _selected.s.r;
}

void scenery_editor::next_rotation()
{
    auto r_1 = (rotation_t)_selected.s.r + 1;
    auto rot = (rotation_)r_1;
    if (rot >= rotation_::COUNT)
        rot = (rotation_)0;
    _selected.s.r = rot;
}

void scenery_editor::prev_rotation()
{
    if (_selected.s.r == (rotation_)0)
        _selected.s.r = (rotation_)((rotation_t)rotation_::COUNT - 1);
    else
        _selected.s.r = (rotation_)((rotation_t)_selected.s.r - 1);
}

} // namespace floormat
