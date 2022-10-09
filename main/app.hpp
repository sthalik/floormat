#pragma once
#include "tile-atlas.hpp"
#include "chunk.hpp"
#include "shaders/tile-shader.hpp"
#include "src/loader.hpp"
#include "floor-mesh.hpp"
#include "wall-mesh.hpp"
#include "wireframe-mesh.hpp"
#include "compat/enum-bitset.hpp"
#include <Magnum/Timeline.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/GL/DebugOutput.h>
#include <memory>

namespace Magnum::Examples {

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
    void do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated);
    void draw_chunk(chunk& c);
    void draw_wireframe();
    void update_window_scale(Vector2i window_size);
    void viewportEvent(ViewportEvent& event) override;
    void debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                        GL::DebugOutput::Severity severity, const std::string& str) const;
    void* register_debug_callback();

    enum class key : int {
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        quit,
        MAX
    };
    chunk make_test_chunk();

    const void* const _dummy = register_debug_callback();
    tile_shader _shader;
    tile_atlas_ floor1 = loader.tile_atlas("share/game/images/metal1.tga", {2, 2});
    tile_atlas_ floor2 = loader.tile_atlas("share/game/images/floor1.tga", {4, 4});
    tile_atlas_ wall1  = loader.tile_atlas("share/game/images/wood2.tga", {2, 2});
    tile_atlas_ wall2  = loader.tile_atlas("share/game/images/wood1.tga", {2, 2});
    chunk _chunk = make_test_chunk();
    floor_mesh _floor_mesh;
    wall_mesh _wall_mesh;
    wireframe_quad_mesh _wireframe_quad;

    Vector2 camera_offset;
    enum_bitset<key> keys;
    Magnum::Timeline timeline;
};

} // namespace Magnum::Examples
