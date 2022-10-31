#include "editor.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/tile-atlas.hpp"
#include "src/loader.hpp"
#include "random.hpp"
#include "compat/assert.hpp"
#include "compat/unreachable.hpp"
#include "src/tile-defs.hpp"
#include "src/world.hpp"
#include <Corrade/Containers/StringView.h>
#include <filesystem>
#include <vector>

namespace floormat {

static const std::filesystem::path image_path{IMAGE_PATH, std::filesystem::path::generic_format};

tile_editor::tile_editor(editor_mode mode, StringView name) : _name{ name}, _mode{ mode}
{
    load_atlases();
}

void tile_editor::load_atlases()
{
    using atlas_array = std::vector<std::shared_ptr<tile_atlas>>;
    for (auto& atlas : json_helper::from_json<atlas_array>(image_path/(_name + ".json")))
    {
        StringView name = atlas->name();
        if (auto x = name.findLast('.'); x)
            name = name.prefix(x.data());
        auto& [_, vec] = _permutation;
        vec.reserve((std::size_t)atlas->num_tiles());
        _atlases[name] = std::move(atlas);
    }
}

std::shared_ptr<tile_atlas> tile_editor::maybe_atlas(StringView str)
{
    auto it = std::find_if(_atlases.begin(), _atlases.end(), [&](const auto& tuple) -> bool {
        const auto& [x, _] = tuple;
        return StringView{x} == str;
    });
    if (it == _atlases.end())
        return nullptr;
    else
        return it->second;
}

std::shared_ptr<tile_atlas> tile_editor::atlas(StringView str)
{
    if (auto ptr = maybe_atlas(str); ptr)
        return ptr;
    else
        fm_abort("no such atlas: %s", str.cbegin());
}

void tile_editor::clear_selection()
{
    _selected_tile = {};
    _permutation = {};
    _selection_mode = sel_none;
}

void tile_editor::select_tile(const std::shared_ptr<tile_atlas>& atlas, std::size_t variant)
{
    fm_assert(atlas);
    clear_selection();
    _selection_mode = sel_tile;
    _selected_tile = { atlas, decltype(tile_image::variant)(variant % atlas->num_tiles()) };
}

void tile_editor::select_tile_permutation(const std::shared_ptr<tile_atlas>& atlas)
{
    fm_assert(atlas);
    clear_selection();
    _selection_mode = sel_perm;
    _permutation = { atlas, {} };
}

bool tile_editor::is_tile_selected(const std::shared_ptr<const tile_atlas>& atlas, std::size_t variant) const
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

tile_image tile_editor::get_selected_perm()
{
    auto& [atlas, vec] = _permutation;
    using variant_t = decltype(tile_image::variant);
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

tile_image tile_editor::get_selected()
{
    switch (_selection_mode)
    {
    case sel_none:
        return {};
    case sel_tile:
        return _selected_tile;
    case sel_perm:
        return get_selected_perm();
    default:
        fm_warn_once("invalid editor mode '%u'", (unsigned)_selection_mode);
        break;
    }
}

void tile_editor::place_tile(world& world, global_coords pos, tile_image& img)
{
    const auto& [c, t] = world[pos];
    const auto& [atlas, variant] = img;
    switch (_mode)
    {
    default:
        fm_warn_once("invalid editor mode '%u'", (unsigned)_mode);
        break;
    case editor_mode::select:
        break;
    case editor_mode::floor:
        t.ground = { atlas, variant };
        break;
    case editor_mode::walls:
        switch (tile_image x = { atlas, variant }; _rotation)
        {
        case editor_wall_rotation::N: t.wall_north = x; break;
        case editor_wall_rotation::W: t.wall_west  = x; break;
        }
        break;
    }
}

void tile_editor::toggle_rotation()
{
    if (_rotation == editor::rotation_W)
        _rotation = editor::rotation_N;
    else
        _rotation = editor::rotation_W;
}

void tile_editor::set_rotation(editor_wall_rotation r)
{
    switch (r)
    {
    default:
        fm_warn_once("invalid rotation '0x%hhx", r);
        return;
    case editor::rotation_W:
    case editor::rotation_N:
        _rotation = r;
        break;
    }
}

editor::editor()
{
    set_mode(editor_mode::floor); // TODO
}

void editor::set_mode(editor_mode mode)
{
    _mode = mode;
    on_release();
}

const tile_editor* editor::current() const noexcept
{
    switch (_mode)
    {
    case editor_mode::select:
        return nullptr;
    case editor_mode::floor:
        return &_floor;
    case editor_mode::walls:
        return &_wall; // todo
    default:
        fm_warn_once("invalid editor mode '%u'", (unsigned)_mode);
        return nullptr;
    }
}

tile_editor* editor::current() noexcept
{
    return const_cast<tile_editor*>(static_cast<const editor&>(*this).current());
}

void editor::on_release()
{
    _last_pos = std::nullopt;
}

void editor::on_mouse_move(world& world, const global_coords pos)
{
    if (_last_pos && *_last_pos != pos)
    {
        _last_pos = pos;
        on_click(world, pos);
    }
}

void editor::on_click(world& world, global_coords pos)
{
    if (auto* mode = current(); mode)
    {
        auto opt = mode->get_selected();
        if (opt)
        {
            _last_pos = pos;
            mode->place_tile(world, pos, opt);
        }
        else
            on_release();
    }
}

} // namespace floormat
