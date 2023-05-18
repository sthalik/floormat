#pragma once
#include "compat/defs.hpp"
#include "global-coords.hpp"
#include "tile-image.hpp"
#include "scenery.hpp"
#include "editor-enums.hpp"
#include "tile-editor.hpp"
#include "scenery-editor.hpp"
#include "vobj-editor.hpp"

#include <map>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>

namespace floormat {

struct world;
struct anim_atlas;
struct tile_atlas;
struct app;

struct editor final
{
    [[nodiscard]] bool dirty() const noexcept { return _dirty; }
    void set_dirty(bool value) noexcept { _dirty = value; }
    [[nodiscard]] editor_mode mode() const noexcept { return _mode; }
    void set_mode(editor_mode mode);

    tile_editor* current_tile_editor() noexcept;
    const tile_editor* current_tile_editor() const noexcept;
    scenery_editor* current_scenery_editor() noexcept;
    const scenery_editor* current_scenery_editor() const noexcept;
    vobj_editor* current_vobj_editor() noexcept;
    const vobj_editor* current_vobj_editor() const noexcept;

    enum class button : unsigned char { none, place, remove, };

    void on_click(world& world, global_coords pos, int mods, button b);
    void on_click_(world& world, global_coords pos, button b);
    void on_mouse_move(world& world, global_coords& pos, int modifiers);
    void on_release();
    void clear_selection();

    editor(app* a);
    editor(editor&&) noexcept = default;
    editor& operator=(editor&&) noexcept = default;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(editor);

    static constexpr inline auto rotation_N = editor_wall_rotation::N;
    static constexpr inline auto rotation_W = editor_wall_rotation::W;
    using snap_mode = editor_snap_mode;

private:
    snap_mode get_snap_value(snap_mode snap, int mods) const;
    static global_coords apply_snap(global_coords pos, global_coords last, snap_mode snap) noexcept;

    app* _app;

    tile_editor _floor{ editor_mode::floor, "floor"_s };
    tile_editor _wall { editor_mode::walls, "wall"_s  };
    scenery_editor _scenery;
    vobj_editor _vobj;

    struct drag_pos final {
        global_coords coord, draw_coord;
        snap_mode snap = snap_mode::none;
        button btn;
    };
    Optional<drag_pos> _last_pos;
    editor_mode _mode = editor_mode::none;
    bool _dirty = false;
};

} // namespace floormat
