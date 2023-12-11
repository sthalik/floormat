#include "tile-editor.hpp"
#include "src/tile-atlas.hpp"
#include "src/world.hpp"
#include "src/random.hpp"
#include "keys.hpp"
#include "loader/loader.hpp"
#include "compat/exception.hpp"
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Utility/Path.h>

namespace floormat {

tile_editor::tile_editor(StringView name) : _name{name}
{
    load_atlases();
}

void tile_editor::load_atlases()
{
    pass_mode default_pass_mode;
    switch (_mode)
    {
    case editor_mode::floor: default_pass_mode = pass_mode::pass; break;
    case editor_mode::walls: default_pass_mode = pass_mode::blocked; break;
    default: default_pass_mode = pass_mode::see_through; break;
    }

    const auto filename = _name + ".json";
    for (const auto& atlas : loader.tile_atlases(filename, default_pass_mode))
    {
        auto& [_, vec] = _permutation;
        vec.reserve(atlas->num_tiles());
        _atlases[atlas->name()] = atlas;
    }
}

std::shared_ptr<tile_atlas> tile_editor::maybe_atlas(StringView str)
{
    if (auto it = _atlases.find(str); it != _atlases.end())
        return it->second;
    else
        return nullptr;
}

std::shared_ptr<tile_atlas> tile_editor::atlas(StringView str)
{
    if (auto ptr = maybe_atlas(str))
        return ptr;
    else
        fm_throw("no such atlas: {}"_cf, str);
}

StringView tile_editor::name() const noexcept { return _name; }

void tile_editor::clear_selection()
{
    _selected_tile = {};
    _permutation = {};
    _selection_mode = sel_none;
}

void tile_editor::select_tile(const std::shared_ptr<tile_atlas>& atlas, size_t variant)
{
    fm_assert(atlas);
    clear_selection();
    _selection_mode = sel_tile;
    _selected_tile = { atlas, variant_t(variant % atlas->num_tiles()) };
}

void tile_editor::select_tile_permutation(const std::shared_ptr<tile_atlas>& atlas)
{
    fm_assert(atlas);
    clear_selection();
    _selection_mode = sel_perm;
    _permutation = { atlas, {} };
}

bool tile_editor::is_tile_selected(const std::shared_ptr<const tile_atlas>& atlas, size_t variant) const
{
    return atlas && _selection_mode == sel_tile && _selected_tile &&
           atlas == _selected_tile.atlas && variant == _selected_tile.variant;
}

bool tile_editor::is_permutation_selected(const std::shared_ptr<const tile_atlas>& atlas) const
{
    const auto& [perm, _] = _permutation;
    return atlas && _selection_mode == sel_perm && perm == atlas;
}

bool tile_editor::is_atlas_selected(const std::shared_ptr<const tile_atlas>& atlas) const
{
    switch (_selection_mode)
    {
    default:
    case sel_none:
        return false;
    case sel_perm:
        return is_permutation_selected(atlas);
    case sel_tile:
        return atlas && _selected_tile && atlas == _selected_tile.atlas;
    }
}

bool tile_editor::is_anything_selected() const
{
    return _selection_mode != sel_none;
}

template<std::random_access_iterator T>
void fisher_yates(T begin, T end)
{
    const auto N = std::distance(begin, end);
    for (auto i = N-1; i >= 1; i--)
    {
        const auto j = random(i+1);
        using std::swap;
        swap(begin[i], begin[j]);
    }
}

tile_image_proto tile_editor::get_selected_perm()
{
    auto& [atlas, vec] = _permutation;
    const auto N = (variant_t)atlas->num_tiles();
    if (N == 0)
        return {};
    if (vec.empty())
    {
        for (variant_t i = 0; i < N; i++)
            vec.push_back(i);
        fisher_yates(vec.begin(), vec.end());
    }
    const auto idx = vec.back();
    vec.pop_back();
    return {atlas, idx};
}

tile_image_proto tile_editor::get_selected()
{
    switch (_selection_mode)
    {
    default:
        fm_warn_once("invalid editor mode '%u'", (unsigned)_selection_mode);
        [[fallthrough]];
    case sel_none:
        return {};
    case sel_tile:
        return _selected_tile;
    case sel_perm:
        return get_selected_perm();
    }
}

void tile_editor::place_tile(world& world, global_coords pos, const tile_image_proto& img)
{
    auto [c, t] = world[pos];
    switch (_mode)
    {
    case editor_mode::none:
        break;
    case editor_mode::floor:
        c.mark_ground_modified();
        t.ground() = img;
        break;
    case editor_mode::walls:
        c.mark_walls_modified();
        switch (_rotation)
        {
        case editor_wall_rotation::N:
            t.wall_north() = img;
            break;
        case editor_wall_rotation::W:
            t.wall_west()  = img;
            break;
        }
        break;
    default:
        fm_warn_once("wrong editor mode '%d' for place_tile()", (int)_mode);
        break;
    }
}

void tile_editor::toggle_rotation()
{
    if (_mode != editor_mode::walls)
        _rotation = editor_wall_rotation::N;
    else if (_rotation == editor_wall_rotation::W)
        _rotation = editor_wall_rotation::N;
    else
        _rotation = editor_wall_rotation::W;
}

void tile_editor::set_rotation(editor_wall_rotation r)
{
    switch (r)
    {
    default:
        fm_warn_once("invalid rotation '0x%hhx", (char)r);
        return;
    case editor_wall_rotation::W:
    case editor_wall_rotation::N:
        if (_mode == editor_mode::walls)
            _rotation = r;
        else
            _rotation = editor_wall_rotation::N;
        break;
    }
}

auto tile_editor::check_snap(int mods) const -> editor_snap_mode
{
    const bool ctrl = mods & kmod_ctrl, shift = mods & kmod_shift;

    if (!(ctrl | shift))
        return editor_snap_mode::none;

    switch (_mode)
    {
    default:
    case editor_mode::none:
        return editor_snap_mode::none;
    case editor_mode::walls:
        switch (_rotation)
        {
        case editor_wall_rotation::N:
            return editor_snap_mode::horizontal;
        case editor_wall_rotation::W:
            return editor_snap_mode::vertical;
        default: return editor_snap_mode::none;
        }
    case editor_mode::floor:
        if (shift)
            return editor_snap_mode::horizontal;
        if (ctrl)
            return editor_snap_mode::vertical;
        return editor_snap_mode::none;
    }
}

bool tile_editor::can_rotate() const
{
    return _mode == editor_mode::walls;
}

} // namespace floormat
