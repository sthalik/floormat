#pragma once
#include "tile-atlas.hpp"
#include <map>
#include <memory>
#include <tuple>
#include <Corrade/Containers/StringView.h>

namespace floormat {

enum class editor_mode : unsigned char {
    select, floors, walls,
};

struct tile_type final
{
    tile_type(Containers::StringView name);
    std::shared_ptr<struct tile_atlas> atlas(Containers::StringView str);
    auto begin() & { return _atlases.begin(); }
    auto end() & { return _atlases.end(); }
    auto begin() const&& { return _atlases.cbegin(); }
    auto end() const&& { return _atlases.cend(); }
    auto cbegin() const { return _atlases.cbegin(); }
    auto cend() const { return _atlases.cend(); }
    Containers::StringView name() const { return _name; }

private:
    std::string _name;
    std::map<std::string, std::shared_ptr<struct tile_atlas>> _atlases;
    void load_atlases();
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
    tile_type _floors{"floor"};
    editor_mode _mode = {};
    bool _dirty = false;
};

} // namespace floormat
