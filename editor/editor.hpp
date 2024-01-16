#pragma once
#include "compat/defs.hpp"
#include "compat/safe-ptr.hpp"
#include "src/global-coords.hpp"
#include "src/tile-image.hpp"
#include "src/scenery.hpp"
#include "editor-enums.hpp"
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>

namespace floormat {

class world;
class anim_atlas;
class ground_atlas;
struct app;

class ground_editor;
class wall_editor;
class scenery_editor;
class vobj_editor;

class editor final
{
    editor_snap_mode get_snap_value(editor_snap_mode snap, int mods) const;
    static global_coords apply_snap(global_coords pos, global_coords last, editor_snap_mode snap) noexcept;

    app* _app;

    safe_ptr<ground_editor> _floor;
    safe_ptr<wall_editor> _wall;
    safe_ptr<scenery_editor> _scenery;
    safe_ptr<vobj_editor> _vobj;

    struct drag_pos final {
        global_coords coord, draw_coord;
        editor_snap_mode snap = editor_snap_mode::none;
        editor_button btn;
    };
    Optional<drag_pos> _last_pos;
    editor_mode _mode = editor_mode::none;
    bool _dirty = false;

public:
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(editor);
    editor(app* a);
    editor(editor&&) noexcept;
    editor& operator=(editor&&) noexcept;
    ~editor() noexcept;

    [[nodiscard]] bool dirty() const noexcept { return _dirty; }
    void set_dirty(bool value) noexcept { _dirty = value; }
    [[nodiscard]] editor_mode mode() const noexcept { return _mode; }
    void set_mode(editor_mode mode);

    ground_editor* current_ground_editor() noexcept;
    const ground_editor* current_ground_editor() const noexcept;
    wall_editor* current_wall_editor() noexcept;
    const wall_editor* current_wall_editor() const noexcept;
    scenery_editor* current_scenery_editor() noexcept;
    const scenery_editor* current_scenery_editor() const noexcept;
    vobj_editor* current_vobj_editor() noexcept;
    const vobj_editor* current_vobj_editor() const noexcept;

    void on_click(world& world, global_coords pos, int mods, editor_button b);
    void on_click_(world& world, global_coords pos, editor_button b);
    void on_mouse_move(world& world, global_coords& pos, int modifiers);
    void on_release();
    void clear_selection();
    Optional<global_coords> mouse_drag_pos();

    using snap_mode = editor_snap_mode;
    using button = editor_button;
};

} // namespace floormat
