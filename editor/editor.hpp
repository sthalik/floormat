#pragma once
#include "compat/defs.hpp"
#include "compat/safe-ptr.hpp"
#include "src/global-coords.hpp"
#include "src/scenery.hpp"
#include "editor-enums.hpp"
#include <array>
#include <cr/Optional.h>
#include <mg/Vector2.h>

namespace floormat {

class world;
class anim_atlas;
class ground_atlas;
class sprite;
struct app;

class ground_editor;
class wall_editor;
class scenery_editor;
class vobj_editor;

struct palette_texture_ref
{
    UnsignedInt id;
    std::array<Vector2, 4> uv; // ImDrawList::AddImageQuad order: TL, TR, BR, BL
};

class editor final
{
    editor_snap_mode get_snap_value(editor_snap_mode snap, int mods) const;
    static global_coords apply_snap(global_coords pos, global_coords last, editor_snap_mode snap) noexcept;

    app* _app;

    safe_ptr<ground_editor> _floor;
    safe_ptr<wall_editor> _wall;
    safe_ptr<scenery_editor> _scenery;
    safe_ptr<vobj_editor> _vobj;

    struct imgui_palette_cache;
    safe_ptr<imgui_palette_cache> _imgui_palette_cache;

    struct drag_pos final {
        global_coords coord, draw_coord;
        editor_snap_mode snap = editor_snap_mode::none;
        editor_button btn;
    };
    Optional<drag_pos> _last_pos;
    editor_mode _mode = editor_mode::none;

public:
    fm_DISABLE_COPY(editor);
    editor(app* a);
    editor(editor&&) noexcept;
    editor& operator=(editor&&) noexcept;
    ~editor() noexcept;

    palette_texture_ref palette_texture(sprite s);
    // Call once per frame, after drawFrame().
    void clear_imgui_palette_cache();

    [[nodiscard]] editor_mode mode() const noexcept;
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
    [[nodiscard]] bool is_dragging() const noexcept;

    using snap_mode = editor_snap_mode;
    using button = editor_button;
};

} // namespace floormat
