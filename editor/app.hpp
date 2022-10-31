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

struct cursor_state final {
    std::optional<Vector2i> pixel;
    std::optional<global_coords> tile;
    bool in_imgui = false;
};

struct app final : floormat_app
{
    static int run_from_argv(int argv, const char* const* argc);

private:
    app(fm_settings&& opts);
    ~app() override;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(app);
    fm_DECLARE_DEPRECATED_MOVE_ASSIGNMENT(app);

    enum class key : unsigned {
        noop,
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        rotate_tile,
        mode_none, mode_floor, mode_walls,
        quit,
        quicksave, quickload,
        COUNT, MIN = noop, NO_REPEAT = rotate_tile, GLOBAL = quicksave,
    };

    void update(float dt) override;
    void maybe_initialize_chunk(const chunk_coords& pos, chunk& c) override;
    void draw_msaa() override;
    void draw() override;

    void on_mouse_move(const mouse_move_event& event) noexcept override;
    void on_mouse_up_down(const mouse_button_event& event, bool is_down) noexcept override;
    void on_mouse_scroll(const mouse_scroll_event& event) noexcept override;
    void on_key_up_down(const key_event& event, bool is_down) noexcept override;
    void on_text_input_event(const text_input_event& event) noexcept override;
    //bool on_text_editing_event(const text_editing_event& event) noexcept override;
    void on_viewport_event(const Magnum::Math::Vector2<int>& size) noexcept override;
    void on_any_event(const any_event& event) noexcept override;
    void on_focus_in() noexcept override;
    void on_focus_out() noexcept override;
    void on_mouse_leave() noexcept override;
    void on_mouse_enter() noexcept override;

    int exec();

    void maybe_initialize_chunk_(const chunk_coords& pos, chunk& c);

    void do_mouse_move();
    void do_mouse_up_down(std::uint8_t button, bool is_down);

    void do_camera(float dt, const enum_bitset<key>& cmds);

    void do_quicksave();
    void do_quickload();

    void reset_camera_offset();
    void update_cursor_tile(const std::optional<Vector2i>& pixel);
    void draw_cursor_tile();

    void init_imgui(Vector2i size);
    void draw_ui();
    float draw_main_menu();
    void draw_fps();
    void draw_cursor_tile_text();
    void render_menu();
    void apply_commands(const enum_bitset<key>& k);
    void clear_non_global_keys();

    void draw_editor_pane(tile_editor& type, float main_menu_height);

    Containers::Pointer<floormat_main> M;
    ImGuiIntegration::Context _imgui{NoCreate};
    std::shared_ptr<tile_atlas> _floor1, _floor2, _wall1, _wall2;
    wireframe_mesh<wireframe::quad_floor> _wireframe_quad;
    wireframe_mesh<wireframe::quad_wall_n> _wireframe_wall_n;
    wireframe_mesh<wireframe::quad_wall_w> _wireframe_wall_w;
    wireframe_mesh<wireframe::box> _wireframe_box;
    editor _editor;
    enum_bitset<key> keys;
    cursor_state cursor;
};

} // namespace floormat
