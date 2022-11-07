#pragma once
#include "compat/defs.hpp"
#include "compat/enum-bitset.hpp"
#include "editor.hpp"
#include "src/global-coords.hpp"
#include "draw/wireframe.hpp"
#include "draw/quad-floor.hpp"
#include "draw/quad-wall-n.hpp"
#include "draw/quad-wall-w.hpp"
#include "draw/box.hpp"
#include "floormat/app.hpp"
#include "keys.hpp"

#include <memory>
#include <optional>

#include <Magnum/ImGuiIntegration/Context.h>
#include <Corrade/Containers/Pointer.h>

namespace floormat {

struct chunk;
struct floormat_main;
struct tile_atlas;
struct tile_editor;
struct fm_settings;
struct anim_atlas;

struct cursor_state final {
    std::optional<Vector2i> pixel;
    std::optional<global_coords> tile;
    bool in_imgui = false;
};

struct app final : floormat_app
{
    static int run_from_argv(int argv, const char* const* argc);
    ~app() override;

private:
    using key_set = enum_bitset<key, key_COUNT>;

    app(fm_settings&& opts);

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(app);
    fm_DECLARE_DEPRECATED_MOVE_ASSIGNMENT(app);

    int exec();

    void update(float dt) override;
    void update_cursor_tile(const std::optional<Vector2i>& pixel);
    void maybe_initialize_chunk(const chunk_coords& pos, chunk& c) override;
    void maybe_initialize_chunk_(const chunk_coords& pos, chunk& c);

    void draw_msaa() override;
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

    void do_quicksave();
    void do_quickload();

    void draw_editor_pane(tile_editor& type, float main_menu_height);
    void draw_cursor();
    void init_imgui(Vector2i size);
    void draw_ui();
    float draw_main_menu();
    void draw_fps();
    void draw_tile_under_cursor();
    void render_menu();

    void do_key(key k, int mods);
    void do_key(key k);
    void apply_commands(const key_set& k);
    static int get_key_modifiers();
    void clear_keys(key min_inclusive, key max_exclusive);
    void clear_keys();
    void clear_non_global_keys() { clear_keys(key_noop, key_GLOBAL); }
    void clear_non_repeated_keys() { clear_keys(key_NO_REPEAT, key_COUNT); }

    Containers::Pointer<floormat_main> M;
    ImGuiIntegration::Context _imgui{NoCreate};
    std::shared_ptr<tile_atlas> _floor1, _floor2, _wall1, _wall2;
    std::shared_ptr<anim_atlas> _door;
    wireframe_mesh<wireframe::quad_floor> _wireframe_quad;
    wireframe_mesh<wireframe::quad_wall_n> _wireframe_wall_n;
    wireframe_mesh<wireframe::quad_wall_w> _wireframe_wall_w;
    wireframe_mesh<wireframe::box> _wireframe_box;
    editor _editor;
    key_set keys;
    std::array<int, key_set::COUNT> key_modifiers;
    cursor_state cursor;
};

} // namespace floormat
