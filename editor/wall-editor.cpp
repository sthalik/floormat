#include "wall-editor.hpp"
#include "src/wall-defs.hpp"
#include "src/wall-atlas.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "loader/wall-info.hpp"
#include <Corrade/Containers/ArrayView.h>

namespace floormat {

using namespace floormat::Wall;

namespace {

struct rot_pair { rotation r; Direction_ d; };

constexpr inline rot_pair rot_map[] = {
    { rotation::N, Direction_::N },
    { rotation::W, Direction_::W },
};
static_assert(std::size(rot_map) == Direction_COUNT);

constexpr rotation dir_to_rot(Direction_ D)
{
    for (auto [r, d] : rot_map)
        if (D == d)
            return r;
    fm_abort("invalid rotation '%d'!", (int)D);
}

constexpr Direction_ rot_to_dir(rotation R)
{
    for (auto [r, d] : rot_map)
        if (r == R)
            return d;
    fm_abort("invalid rotation '%d'!", (int)R);
}

constexpr rotation next_rot(rotation r)
{
    auto dir_0 = (unsigned)rot_to_dir(r);
    auto dir_1 = (dir_0 + 1) % Direction_COUNT;
    return dir_to_rot((Direction_)dir_1);
}
static_assert(next_rot(rotation::N) == rotation::W);
static_assert(next_rot(rotation::W) == rotation::N);

} // namespace

void wall_editor::load_atlases()
{
    fm_assert(_atlases.empty());
    for (const auto& wa : loader.wall_atlas_list())
        _atlases[wa.name] = &wa;
    fm_assert(!_atlases.empty());
}

wall_editor::wall_editor()
{
    load_atlases();
}

StringView wall_editor::name() const { return "wall"_s; }
enum rotation wall_editor::rotation() const { return _r; }
void wall_editor::set_rotation(enum rotation r) { _r = r; }
void wall_editor::toggle_rotation() { _r = next_rot(_r); }
std::shared_ptr<wall_atlas> wall_editor::get_selected() const { return _selected_atlas; }
void wall_editor::select_atlas(const std::shared_ptr<wall_atlas>& atlas) { _selected_atlas = atlas; }
void wall_editor::clear_selection() { _selected_atlas = nullptr; }
bool wall_editor::is_atlas_selected(const std::shared_ptr<wall_atlas>& atlas) const { return _selected_atlas == atlas; }
bool wall_editor::is_anything_selected() const { return _selected_atlas != nullptr; }

void wall_editor::place_tile(world& w, global_coords coords, const std::shared_ptr<wall_atlas>& atlas)
{
    auto [c, t] = w[coords];
    switch (_r)
    {
    case rotation::N: t.wall_north() = { atlas, (variant_t)-1 }; break;
    case rotation::W: t.wall_west() = { atlas, (variant_t)-1 }; break;
    default: std::unreachable();
    }
    //c.mark_walls_modified();
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            if (auto* ch = w.at(coords + Vector2i(x, y)))
                ch->mark_walls_modified();
}

editor_snap_mode wall_editor::check_snap(int mods) const
{
    (void)mods;
    if (!is_anything_selected())
        return editor_snap_mode::none;
    if (_r == rotation::N)
        return editor_snap_mode::horizontal;
    else if (_r == rotation::W)
        return editor_snap_mode::vertical;
    else
        std::unreachable();
}

} // namespace floormat
