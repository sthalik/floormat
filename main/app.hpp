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

struct app final : Platform::Application
{
    using dpi_policy = Platform::Implementation::Sdl2DpiScalingPolicy;
    using tile_atlas_ = std::shared_ptr<tile_atlas>;

    explicit app(const Arguments& arguments);
    virtual ~app();

    void update(float dt);

    void reset_camera_offset();
    void update_window_scale(Vector2i window_size);

    void do_camera(float dt);
    void do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated);

    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;
    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;
    void textInputEvent(TextInputEvent& event) override;
    void viewportEvent(ViewportEvent& event) override;
    void anyEvent(SDL_Event& event) override;
    void event_leave();
    void event_enter();
    void event_mouse_enter();
    void event_mouse_leave();

    void drawEvent() override;
    std::array<std::int16_t, 4> get_draw_bounds() const noexcept;
    void draw_world();
    void draw_wireframe_quad(global_coords pt);
    void draw_wireframe_box(local_coords pt);

    void do_menu();
    void draw_menu_(tile_type& type, float main_menu_height);
    void draw_fps(float main_menu_height);
    void setup_menu();
    void display_menu();

    void debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                        GL::DebugOutput::Severity severity, const std::string& str) const;
    void* register_debug_callback();

    global_coords pixel_to_tile(Vector2 position) const;
    void draw_cursor_tile();
    void do_mouse_click(global_coords pos, int button);

    std::optional<Vector2i> _cursor_pos;

    static constexpr Vector2 project(Vector3 pt);
    static constexpr Vector2 unproject(Vector2 px);

    enum class key : int {
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        quit,
        MAX
    };
    void make_test_chunk(chunk& c);

    const void* _dummy = register_debug_callback();
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
    Vector2 camera_offset;
    enum_bitset<key> keys;
    Magnum::Timeline timeline;
    editor _editor;

    static constexpr std::int32_t BASE_X = 0, BASE_Y = 0;
};

constexpr Vector2 app::project(const Vector3 pt)
{
    const float x = -pt[1], y = -pt[0], z = pt[2];
    return { x-y, (x+y+z*2)*.59f };
}

constexpr Vector2 app::unproject(const Vector2 px)
{
    const float X = px[0], Y = px[1];
    return { X/2 + 50.f * Y / 59, 50 * Y / 59 - X/2 };
}

} // namespace floormat
