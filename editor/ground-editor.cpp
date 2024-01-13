#include "ground-editor.hpp"
#include "src/ground-atlas.hpp"
#include "src/world.hpp"
#include "src/random.hpp"
#include "loader/ground-info.hpp"
#include "keys.hpp"
#include "loader/loader.hpp"
#include "compat/exception.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat {

ground_editor::ground_editor()
{
    load_atlases();
}

void ground_editor::load_atlases()
{
    fm_assert(_atlases.empty());
    for (const auto& g : loader.ground_atlas_list())
        _atlases[g.name] = &g;
    fm_assert(!_atlases.empty());
}

std::shared_ptr<ground_atlas> ground_editor::maybe_atlas(StringView str)
{
    if (auto it = _atlases.find(str); it != _atlases.end())
        return it->second->atlas;
    else
        return nullptr;
}

std::shared_ptr<ground_atlas> ground_editor::atlas(StringView str)
{
    if (auto ptr = maybe_atlas(str))
        return ptr;
    else
        fm_throw("no such atlas: {}"_cf, str);
}

StringView ground_editor::name() const noexcept { return "ground"_s; }

void ground_editor::clear_selection()
{
    _selected_tile = {};
    _permutation = {};
    _selection_mode = sel_none;
}

void ground_editor::select_tile(const std::shared_ptr<ground_atlas>& atlas, size_t variant)
{
    fm_assert(atlas);
    clear_selection();
    _selection_mode = sel_tile;
    _selected_tile = { atlas, variant_t(variant % atlas->num_tiles()) };
}

void ground_editor::select_tile_permutation(const std::shared_ptr<ground_atlas>& atlas)
{
    fm_assert(atlas);
    clear_selection();
    _selection_mode = sel_perm;
    _permutation = { atlas, {} };
}

bool ground_editor::is_tile_selected(const std::shared_ptr<const ground_atlas>& atlas, size_t variant) const
{
    return atlas && _selection_mode == sel_tile && _selected_tile &&
           atlas == _selected_tile.atlas && variant == _selected_tile.variant;
}

bool ground_editor::is_permutation_selected(const std::shared_ptr<const ground_atlas>& atlas) const
{
    const auto& [perm, _] = _permutation;
    return atlas && _selection_mode == sel_perm && perm == atlas;
}

bool ground_editor::is_atlas_selected(const std::shared_ptr<const ground_atlas>& atlas) const
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

bool ground_editor::is_anything_selected() const
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

tile_image_proto ground_editor::get_selected_perm()
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

tile_image_proto ground_editor::get_selected()
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

void ground_editor::place_tile(world& world, global_coords pos, const tile_image_proto& img)
{
    auto [c, t] = world[pos];
    c.mark_ground_modified();
    t.ground() = img;
}

auto ground_editor::check_snap(int mods) const -> editor_snap_mode
{
    const bool ctrl = mods & kmod_ctrl, shift = mods & kmod_shift;

    if (!(ctrl | shift))
        return editor_snap_mode::none;

    if (shift)
        return editor_snap_mode::horizontal;
    if (ctrl)
        return editor_snap_mode::vertical;
    return editor_snap_mode::none;
}

} // namespace floormat
