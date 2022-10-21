#pragma once
#include "compat/defs.hpp"
#include "tile-atlas.hpp"
#include "global-coords.hpp"
#include "tile.hpp"

#include <cstdint>
#include <tuple>
#include <optional>
#include <vector>
#include <map>
#include <memory>
#include <Corrade/Containers/StringView.h>

namespace floormat {

enum class editor_mode : unsigned char {
    select, floor, walls,
};

struct world;

struct tile_type final
{
    tile_type(editor_mode mode, Containers::StringView name);
    std::shared_ptr<tile_atlas> maybe_atlas(Containers::StringView str);
    std::shared_ptr<tile_atlas> atlas(Containers::StringView str);
    auto begin() & { return _atlases.begin(); }
    auto end() & { return _atlases.end(); }
    auto begin() const&& { return _atlases.cbegin(); }
    auto end() const&& { return _atlases.cend(); }
    auto cbegin() const { return _atlases.cbegin(); }
    auto cend() const { return _atlases.cend(); }
    Containers::StringView name() const { return _name; }
    editor_mode mode() const { return _mode; }

    void clear_selection();
    void select_tile(const std::shared_ptr<tile_atlas>& atlas, std::size_t variant);
    void select_tile_permutation(const std::shared_ptr<tile_atlas>& atlas);
    bool is_tile_selected(const std::shared_ptr<const tile_atlas>& atlas, std::size_t variant) const;
    bool is_permutation_selected(const std::shared_ptr<const tile_atlas>& atlas) const;
    tile_image get_selected();
    void place_tile(world& world, global_coords pos, tile_image& img);

private:
    enum selection_mode : std::uint8_t {
        sel_none, sel_tile, sel_perm,
    };
    enum rotation : std::uint8_t {
        rot_N, rot_W,
    };

    std::string _name;
    std::map<std::string, std::shared_ptr<tile_atlas>> _atlases;
    tile_image _selected_tile;
    std::tuple<std::shared_ptr<tile_atlas>, std::vector<std::size_t>> _permutation;
    selection_mode _selection_mode = sel_none;
    editor_mode _mode;
    rotation _rotation{};

    void load_atlases();
    tile_image get_selected_perm();
};

struct editor final
{
    [[nodiscard]] bool dirty() const { return _dirty; }
    void set_dirty(bool value) { _dirty = value; }
    [[nodiscard]] editor_mode mode() const { return _mode; }
    void set_mode(editor_mode mode);

    tile_type& floor() { return _floor; }
    const tile_type& floor() const { return _floor; }

    tile_type* current();
    const tile_type* current() const;

    void on_click(world& world, global_coords pos);
    void on_mouse_move(world& world, const global_coords pos);
    void on_release();

    editor();
    editor(editor&&) noexcept = default;
    editor& operator=(editor&&) noexcept = default;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(editor);

private:
    tile_type _floor{editor_mode::floor, "floor"};
    std::optional<global_coords> _last_pos;
    editor_mode _mode = editor_mode::select;
    bool _dirty = false;
};

} // namespace floormat
