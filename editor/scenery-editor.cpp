#include "scenery-editor.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/RTree-search.hpp"
#include "src/rotation.inl"
#include "app.hpp"
#include <Magnum/Math/Range.h>

namespace floormat {

using rotation_t = std::underlying_type_t<enum rotation>;

scenery_editor::scenery_::operator bool() const noexcept
{
    return !!proto;
}

scenery_editor::scenery_editor() noexcept
{
    load_atlases();
}

void scenery_editor::set_rotation(enum rotation r)
{
    auto& s = _selected.proto;
    s.offset = rotate_point(s.offset, s.r, r);
    s.bbox_offset = rotate_point(s.bbox_offset, s.r, r);
    s.bbox_size = rotate_size(s.bbox_size, s.r, r);
    s.r = r;
}

#if 0
enum rotation scenery_editor::rotation() const
{
    return _selected.proto.r;
}
#endif

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

void scenery_editor::place_tile(world& w, global_coords pos, const scenery_& s, app& a)
{
    if (!s)
    {
        auto [c, t] = w[pos];
start:
        const auto& es = c.objects();
        const auto sz = es.size();
        while (auto id = a.get_object_colliding_with_cursor())
        {
            for (auto i = 0uz; i < sz; i++)
                if (es[i]->id == id)
                {
                    c.remove_object(i);
                    goto start;
                }
            break;
        }
    }
    else
        // todo check collision at pos
        w.make_object<scenery>(w.make_id(), pos, s.proto);
}

} // namespace floormat
