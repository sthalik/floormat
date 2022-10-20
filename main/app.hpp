#pragma once
#include "tile-atlas.hpp"
#include "chunk.hpp"
#include "shaders/tile-shader.hpp"
#include "src/loader.hpp"
#include "draw/floor-mesh.hpp"
#include "draw/wall-mesh.hpp"
#include "draw/wireframe-mesh.hpp"
#include "draw/wireframe-quad.hpp"
#include "draw/wireframe-box.hpp"
#include "compat/enum-bitset.hpp"
#include "editor.hpp"
#include "world.hpp"
#include <Magnum/Timeline.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/ImGuiIntegration/Context.h>
#include <memory>

namespace floormat {

struct app final : private Platform::Application
{
    static int run_from_argv(int argc, char** argv);
    virtual ~app();

private:
    struct app_settings;

    [[maybe_unused]] [[noreturn]] static void usage(const Utility::Arguments& args);
    explicit app(const Arguments& arguments, app_settings opts);

    using dpi_policy = Platform::Implementation::Sdl2DpiScalingPolicy;
    using tile_atlas_ = std::shared_ptr<tile_atlas>;

    void update(float dt);

    void do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated);
    void do_mouse_click(global_coords pos, int button);

    void do_camera(float dt);
    void reset_camera_offset();
    void update_window_scale(Vector2i window_size);
    void recalc_cursor_tile();

    void viewportEvent(ViewportEvent& event) override;
    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;
    void textInputEvent(TextInputEvent& event) override;
    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;
    void anyEvent(SDL_Event& event) override;

    void event_focus_out();
    void event_focus_in();
    void event_mouse_enter();
    void event_mouse_leave();

    std::array<std::int16_t, 4> get_draw_bounds() const noexcept;
    void drawEvent() override;
    void draw_world();
    void draw_cursor_tile();
    void draw_wireframe_quad(global_coords pt);
    void draw_wireframe_box(local_coords pt);

    void draw_ui();
    float draw_main_menu();
    void draw_editor_pane(tile_type& type, float main_menu_height);
    void draw_fps();
    void draw_cursor_coord();
    void display_menu();

    void debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                        GL::DebugOutput::Severity severity, const std::string& str) const;
    static void _debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                                GL::DebugOutput::Severity severity, const std::string& str, const void* self);
    void* register_debug_callback();

    global_coords pixel_to_tile(Vector2d position) const;

    enum class key : int {
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        quit,
        MAX
    };
    void make_test_chunk(chunk& c);

    [[maybe_unused]] void* _dummy = register_debug_callback();

    tile_shader _shader;
    tile_atlas_ floor1 = loader.tile_atlas("floor-tiles.tga", {44, 4});
    tile_atlas_ floor2 = loader.tile_atlas("metal1.tga", {2, 2});
    tile_atlas_ wall1  = loader.tile_atlas("wood2.tga", {2, 2});
    tile_atlas_ wall2  = loader.tile_atlas("wood1.tga", {2, 2});

    floor_mesh _floor_mesh;
    wall_mesh _wall_mesh;
    wireframe_mesh<wireframe::quad> _wireframe_quad;
    wireframe_mesh<wireframe::box> _wireframe_box;

    ImGuiIntegration::Context _imgui{NoCreate};

    world _world;
    enum_bitset<key> keys;
    Magnum::Timeline timeline;
    editor _editor;
    std::optional<Vector2i> _cursor_pixel;
    std::optional<global_coords> _cursor_tile;
    float _frame_time = 0;
    bool _cursor_in_imgui = false;

    struct app_settings {
        bool vsync = true;
    };

    app_settings _settings;

    static constexpr std::int16_t BASE_X = 0, BASE_Y = 0;
};

} // namespace floormat
