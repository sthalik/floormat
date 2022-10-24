#pragma once
#include "compat/defs.hpp"
#include "compat/enum-bitset.hpp"
#include "editor.hpp"
#include "src/global-coords.hpp"
#include "draw/wireframe-mesh.hpp"
#include "draw/wireframe-quad.hpp"
#include "draw/wireframe-box.hpp"
#include "main/floormat-app.hpp"

#include <memory>
#include <optional>

#include <Magnum/ImGuiIntegration/Context.h>
#include <Corrade/Containers/Pointer.h>

namespace floormat {

struct chunk;
struct floormat_main;
struct tile_atlas;
struct tile_editor;

struct cursor_state final {
    std::optional<Vector2i> pixel;
    std::optional<global_coords> tile;
    bool in_imgui = false;
};

struct app final : floormat_app
{
    app();
    ~app() override;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(app);
    fm_DECLARE_DEPRECATED_MOVE_ASSIGNMENT(app);

    void update(float dt) override;
    void maybe_init_chunk(const chunk_coords& pos, chunk& c) override;
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

    static int run_from_argv(int argv, const char* const* argc);

private:
    using tile_atlas_ = std::shared_ptr<tile_atlas>;

    enum class key : int {
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        rotate_tile, quicksave, quickload,
        quit,
        MAX = quit, COUNT
    };

    void do_mouse_click(global_coords pos, int button);
    void do_mouse_release(int button);
    void do_mouse_move(global_coords pos);

    void do_camera(float dt);
    void reset_camera_offset();
    void recalc_cursor_tile();
    void init_imgui(Vector2i size);

    void draw_ui();
    float draw_main_menu();
    void draw_fps();
    void draw_cursor_tile_coord();
    void render_menu();

    void draw_editor_pane(tile_editor& type, float main_menu_height);

    void draw_cursor_tile();

    void draw_wireframe_quad(global_coords pt);
    void draw_wireframe_box(global_coords pt);

    Containers::Pointer<floormat_main> M;
    std::shared_ptr<tile_atlas> _floor1, _floor2, _wall1, _wall2;
    ImGuiIntegration::Context _imgui{NoCreate};
    wireframe_mesh<wireframe::quad> _wireframe_quad;
    wireframe_mesh<wireframe::box> _wireframe_box;
    editor _editor;
    enum_bitset<key> _keys;
    cursor_state cursor;
};

} // namespace floormat
