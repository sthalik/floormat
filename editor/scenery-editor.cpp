#include "scenery-editor.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "src/world.hpp"
#include "rotation.inl"

namespace floormat {

using rotation_ = rotation;
using rotation_t = std::underlying_type_t<rotation_>;

scenery_editor::scenery_::operator bool() const noexcept
{
    return proto;
}

scenery_editor::scenery_editor() noexcept
{
    load_atlases();
}

void scenery_editor::set_rotation(rotation_ r)
{
    auto& s = _selected.proto;
    s.bbox_offset = rotate_point(s.bbox_offset, s.r, r);
    s.bbox_size = rotate_size(s.bbox_size, s.r, r);
    s.r = r;
}

rotation_ scenery_editor::rotation() const
{
    return _selected.proto.r;
}

void scenery_editor::next_rotation()
{
    set_rotation(_selected.proto.atlas->next_rotation_from(_selected.proto.r));
}

void scenery_editor::prev_rotation()
{
    set_rotation(_selected.proto.atlas->prev_rotation_from(_selected.proto.r));
}

void scenery_editor::select_tile(const scenery_& s)
{
    const auto r = s.proto.atlas && s.proto.atlas->check_rotation(_selected.proto.r)
                   ? _selected.proto.r
                   : s.proto.r;
    _selected = s;
    set_rotation(r);
}

void scenery_editor::clear_selection()
{
    _selected = {};
}

auto scenery_editor::get_selected() const -> const scenery_&
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

void scenery_editor::place_tile(world& w, global_coords pos, const scenery_& s)
{
    auto [c, t] = w[pos];
    // todo check collision at pos
    auto sc = w.make_entity<scenery>(pos, s.proto);
    c.mark_scenery_modified();
}

} // namespace floormat
