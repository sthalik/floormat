#include "scenery-editor.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "src/world.hpp"
#include "rotation.inl"

namespace floormat {

using rotation_t = std::underlying_type_t<enum rotation>;

scenery_editor::scenery_::operator bool() const noexcept
{
    return proto;
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

enum rotation scenery_editor::rotation() const
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
    if (!s)
    {
        // don't regen colliders
        const auto px = Vector2(pos.local()) * TILE_SIZE2;
        const auto es = c.entities();
        for (auto i = es.size()-1; i != (size_t)-1; i--)
        {
            const auto& e = *es[i];
            if (e.type() != entity_type::scenery)
                continue;
            auto center = Vector2(e.coord.local())*TILE_SIZE2 + Vector2(e.offset) + Vector2(e.bbox_offset),
                 min = center - Vector2(e.bbox_size/2), max = min + Vector2(e.bbox_size);
            if (px >= min && px <= max)
                c.remove_entity(i);
        }
    }
    else
        // todo check collision at pos
        w.make_entity<scenery>(w.make_id(), pos, s.proto);
}

} // namespace floormat
