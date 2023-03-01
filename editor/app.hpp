#pragma once
#include "compat/defs.hpp"
#include "compat/enum-bitset.hpp"
#include "editor.hpp"
#include "draw/wireframe.hpp"
#include "draw/quad-floor.hpp"
#include "draw/quad-wall-n.hpp"
#include "draw/quad-wall-w.hpp"
#include "draw/quad.hpp"
#include "draw/box.hpp"
#include "floormat/app.hpp"
#include "keys.hpp"
#include <memory>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/Optional.h>
#include <Magnum/ImGuiIntegration/Context.h>

namespace floormat {

struct chunk;
struct floormat_main;
struct tile_atlas;
struct tile_editor;
struct fm_settings;
struct anim_atlas;

struct cursor_state final {
    Optional<Vector2i> pixel;
    Optional<global_coords> tile;

    bool in_imgui = false;
};

struct clickable;

enum class Cursor: std::uint32_t {
    Arrow, TextInput, Wait, Crosshair, WaitArrow,
    ResizeNWSE, ResizeNESW, ResizeWE, ResizeNS, ResizeAll,
    No, Hand, Hidden, HiddenLocked,
};

struct app final : floormat_app
{
    static int run_from_argv(int argc, const char* const* argv);
    static fm_settings parse_cmdline(int argc, const char* const* argv);
    ~app() override;
#ifdef _WIN32
    static void set_dpi_aware();
#endif

private:
    using key_set = enum_bitset<key, key_COUNT>;

    app(fm_settings&& opts);

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(app);
    fm_DECLARE_DEPRECATED_MOVE_ASSIGNMENT(app);

    int exec();

    void update(float dt) override;
    void update_world(float dt);
    void update_cursor_tile(const Optional<Vector2i>& pixel);
    void maybe_initialize_chunk(const chunk_coords& pos, chunk& c) override;
    void maybe_initialize_chunk_(const chunk_coords& pos, chunk& c);

    void draw() override;

    void on_mouse_move(const mouse_move_event& event) noexcept override;
    void on_mouse_up_down(const mouse_button_event& event, bool is_down) noexcept override;
    void on_mouse_scroll(const mouse_scroll_event& event) noexcept override;
    void on_key_up_down(const key_event& event, bool is_down) noexcept override;
    std::tuple<key, int> resolve_keybinding(int k, int mods) const;
    void on_text_input_event(const text_input_event& event) noexcept override;
    //bool on_text_editing_event(const text_editing_event& event) noexcept override;
    void on_viewport_event(const Magnum::Math::Vector2<int>& size) noexcept override;
    void on_any_event(const any_event& event) noexcept override;
    void on_focus_in() noexcept override;
    void on_focus_out() noexcept override;
    void on_mouse_leave() noexcept override;
    void on_mouse_enter() noexcept override;

    void do_mouse_move(int modifiers);
    void do_mouse_up_down(std::uint8_t button, bool is_down, int modifiers);

    void do_camera(float dt, const key_set& cmds, int mods);
    void reset_camera_offset();
    clickable* find_clickable_scenery(const Optional<Vector2i>& pixel);

    void do_quicksave();
    void do_quickload();
    void do_new_file();
    void do_escape();

    void draw_collision_boxes();
    void draw_editor_pane(float main_menu_height);
    void draw_inspector();
    void draw_editor_tile_pane_atlas(tile_editor& ed, StringView name, const std::shared_ptr<tile_atlas>& atlas);
    void draw_editor_scenery_pane(scenery_editor& ed);
    void set_cursor_from_imgui();
    void draw_cursor();
    void init_imgui(Vector2i size);
    void draw_ui();
    float draw_main_menu();
    void draw_fps();
    void draw_tile_under_cursor();
    void render_menu();

    void do_key(key k, int mods);
    void do_key(key k);
    void do_rotate(bool backward);
    void apply_commands(const key_set& k);
    int get_key_modifiers();
    void clear_keys(key min_inclusive, key max_exclusive);
    void clear_keys();
    void clear_non_global_keys();
    void clear_non_repeated_keys();

    Containers::Pointer<floormat_main> M;
    ImGuiIntegration::Context _imgui{NoCreate};
    std::shared_ptr<tile_atlas> _floor1, _floor2, _wall1, _wall2;
    std::shared_ptr<anim_atlas> _door, _table, _control_panel;
    GL::Texture2D _wireframe_texture = wireframe::make_constant_texture();
    wireframe_mesh<wireframe::quad_floor>  _wireframe_quad   {_wireframe_texture};
    wireframe_mesh<wireframe::quad_wall_n> _wireframe_wall_n {_wireframe_texture};
    wireframe_mesh<wireframe::quad_wall_w> _wireframe_wall_w {_wireframe_texture};
    wireframe_mesh<wireframe::box>         _wireframe_box    {_wireframe_texture};
    wireframe_mesh<wireframe::quad>        _wireframe_rect   {_wireframe_texture};
    editor _editor;
    key_set keys;
    std::array<int, key_set::COUNT> key_modifiers = {};
    cursor_state cursor;
    Optional<global_coords> inspected_scenery;
    bool _enable_render_bboxes : 1 = false;
};

} // namespace floormat
