#pragma once
#include "compat/defs.hpp"
#include "tile-atlas.hpp"
#include "global-coords.hpp"
#include "tile-image.hpp"

#include <cstdint>
#include <tuple>
#include <optional>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <Corrade/Containers/StringView.h>

namespace floormat {

enum class editor_mode : unsigned char {
    select, floor, walls,
};

enum class editor_wall_rotation : std::uint8_t {
    N, W,
};

struct world;

struct tile_editor final
{
private:
    enum selection_mode : std::uint8_t {
        sel_none, sel_tile, sel_perm,
    };

    std::string _name;
    std::map<std::string, std::shared_ptr<tile_atlas>> _atlases;
    tile_image _selected_tile;
    std::tuple<std::shared_ptr<tile_atlas>, std::vector<decltype(tile_image::variant)>> _permutation;
    selection_mode _selection_mode = sel_none;
    editor_mode _mode;
    editor_wall_rotation _rotation = editor_wall_rotation::N;

    void load_atlases();
    tile_image get_selected_perm();

public:
    tile_editor(editor_mode mode, StringView name);
    std::shared_ptr<tile_atlas> maybe_atlas(StringView str);
    std::shared_ptr<tile_atlas> atlas(StringView str);
    auto cbegin() const noexcept { return _atlases.cbegin(); }
    auto cend() const noexcept { return _atlases.cend(); }
    auto begin() const noexcept { return _atlases.cbegin(); }
    auto end() const noexcept { return _atlases.cend(); }
    StringView name() const noexcept { return _name; }
    editor_mode mode() const noexcept { return _mode; }
    editor_wall_rotation rotation() const noexcept { return _rotation; }

    void clear_selection();
    void select_tile(const std::shared_ptr<tile_atlas>& atlas, std::size_t variant);
    void select_tile_permutation(const std::shared_ptr<tile_atlas>& atlas);
    bool is_tile_selected(const std::shared_ptr<const tile_atlas>& atlas, std::size_t variant) const;
    bool is_permutation_selected(const std::shared_ptr<const tile_atlas>& atlas) const;
    bool is_atlas_selected(const std::shared_ptr<const tile_atlas>& atlas) const;
    tile_image get_selected();
    void place_tile(world& world, global_coords pos, tile_image& img);
    void toggle_rotation();
    void set_rotation(editor_wall_rotation r);
};

struct editor final
{
    [[nodiscard]] bool dirty() const noexcept { return _dirty; }
    void set_dirty(bool value) noexcept { _dirty = value; }
    [[nodiscard]] editor_mode mode() const noexcept { return _mode; }
    void set_mode(editor_mode mode);

    tile_editor* current() noexcept;
    const tile_editor* current() const noexcept;

    void on_click(world& world, global_coords pos);
    void on_mouse_move(world& world, global_coords pos);
    void on_release();

    editor();
    editor(editor&&) noexcept = default;
    editor& operator=(editor&&) noexcept = default;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(editor);

    using rotation = editor_wall_rotation;
    static constexpr inline auto rotation_N = editor_wall_rotation::N;
    static constexpr inline auto rotation_W = editor_wall_rotation::W;

private:
    tile_editor _floor{ editor_mode::floor, "floor"};
    tile_editor _wall{ editor_mode::walls, "wall"};
    std::optional<global_coords> _last_pos;
    editor_mode _mode = editor_mode::select;
    bool _dirty = false;
};

} // namespace floormat
