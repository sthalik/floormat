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
    void drawEvent() override;
    void update(float dt);
    void do_camera(float dt);
    void reset_camera_offset();
    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;
    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;
    void textInputEvent(TextInputEvent& event) override;
    void do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated);
    void draw_chunk(chunk& c);
    void draw_wireframe_quad();
    void draw_wireframe_box();
    void update_window_scale(Vector2i window_size);
    void viewportEvent(ViewportEvent& event) override;
    void draw_menu();
    void draw_menu_(tile_type& type, float main_menu_height);
    void setup_menu();
    void display_menu();
    void debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                        GL::DebugOutput::Severity severity, const std::string& str) const;
    void* register_debug_callback();

    enum class key : int {
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        quit,
        MAX
    };
    chunk make_test_chunk();

    const void* _dummy = register_debug_callback();
    tile_shader _shader;
    tile_atlas_ floor1 = loader.tile_atlas("metal1.tga", {2, 2});
    tile_atlas_ floor2 = loader.tile_atlas("floor1.tga", {4, 4});
    tile_atlas_ wall1  = loader.tile_atlas("wood2.tga", {2, 2});
    tile_atlas_ wall2  = loader.tile_atlas("wood1.tga", {2, 2});
    chunk _chunk = make_test_chunk();

    floor_mesh _floor_mesh;
    wall_mesh _wall_mesh;
    wireframe_mesh<wireframe::quad> _wireframe_quad;
    wireframe_mesh<wireframe::box> _wireframe_box;

    ImGuiIntegration::Context _imgui{NoCreate};

    Vector2 camera_offset;
    enum_bitset<key> keys;
    Magnum::Timeline timeline;
    editor_state _editor;
};

} // namespace floormat
