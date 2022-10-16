#pragma once
#include "tile-atlas.hpp"
#include <map>
#include <memory>
#include <tuple>
#include <optional>
#include <Corrade/Containers/StringView.h>

namespace floormat {

enum class editor_mode : unsigned char {
    select, floors, walls,
};

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
    void select_tile(const std::shared_ptr<tile_atlas>& atlas, std::uint8_t variant);
    void select_tile_permutation(const std::shared_ptr<tile_atlas>& atlas);
    bool is_tile_selected(const std::shared_ptr<tile_atlas>& atlas, std::uint8_t variant);
    bool is_permutation_selected(const std::shared_ptr<tile_atlas>& atlas);
    std::optional<std::tuple<std::shared_ptr<tile_atlas>, std::uint8_t>> get_selected();

private:
    enum selection_mode : std::uint8_t {
        sel_none, sel_tile, sel_perm,
    };
    enum rotation : std::uint8_t {
        rot_N, rot_W,
    };

    std::string _name;
    std::map<std::string, std::shared_ptr<tile_atlas>> _atlases;
    std::tuple<std::shared_ptr<tile_atlas>, std::uint32_t> _selected_tile;
    std::tuple<std::shared_ptr<tile_atlas>, std::vector<std::uint8_t>> _permutation;
    selection_mode _selection_mode = sel_none;
    editor_mode _mode;
    rotation _rotation{};

    void load_atlases();
    std::tuple<std::shared_ptr<tile_atlas>, std::uint8_t> get_selected_perm();
};

struct editor_state final
{
    [[nodiscard]] bool dirty() const { return _dirty; }
    void set_dirty(bool value) { _dirty = value; }
    [[nodiscard]] editor_mode mode() const { return _mode; }
    void set_mode(editor_mode mode) { _mode = mode; }

    tile_type& floors() { return _floors; }
    const tile_type& floors() const { return _floors; }

    editor_state();

    editor_state(const editor_state&) = delete;
    editor_state& operator=(const editor_state&) = delete;

    editor_state(editor_state&&) noexcept = default;
    editor_state& operator=(editor_state&&) noexcept = default;

private:
    tile_type _floors{editor_mode::floors, "floor"};
    editor_mode _mode = {};
    bool _dirty = false;
};

} // namespace floormat
