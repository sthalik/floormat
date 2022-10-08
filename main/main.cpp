#include "app.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Trade/AbstractImporter.h>

namespace Magnum::Examples {

chunk app::make_test_chunk()
{
    constexpr auto N = TILE_MAX_DIM;
    chunk c;
    for (auto [x, k, pt] : c) {
        const auto& atlas = pt.x > N/2 && pt.y >= N/2 ? floor2 : floor1;
        x.ground_image = { atlas, (std::uint8_t)(k % atlas->num_tiles().product()) };
    }
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north = { wall1, 0 };
    c[{K,   K  }].wall_west  = { wall2, 0 };
    c[{K,   K+1}].wall_north = { wall1, 0 };
    c[{K+1, K  }].wall_west  = { wall2, 0 };
    return c;
}

void app::drawEvent() {
#if 0
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
    GL::Renderer::setDepthMask(true);
    GL::Renderer::setDepthFunction(GL::Renderer::DepthFunction::LessOrEqual);
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
#else
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);
    GL::Renderer::setDepthFunction(GL::Renderer::DepthFunction::Never);
#endif

    update_window_scale();
    {
        float dt = timeline.previousFrameDuration();
        update(dt);
    }

    draw_chunk(_chunk);

    swapBuffers();
    redraw();
    timeline.nextFrame();
}

void app::draw_chunk(chunk& c)
{
    _floor_mesh.draw(_shader, c);
    _wall_mesh.draw(_shader, c);
}

void app::do_camera(float dt)
{
    constexpr float pixels_per_second = 512;
    if (keys[key::camera_up])
        camera_offset += Vector2(0, 1) * dt * pixels_per_second;
    else if (keys[key::camera_down])
        camera_offset += Vector2(0, -1)  * dt * pixels_per_second;
    if (keys[key::camera_left])
        camera_offset += Vector2(1, 0) * dt * pixels_per_second;
    else if (keys[key::camera_right])
        camera_offset += Vector2(-1, 0)  * dt * pixels_per_second;

    _shader.set_camera_offset(camera_offset);

    if (keys[key::camera_reset])
        reset_camera_offset();
}

void app::reset_camera_offset()
{
    camera_offset = _shader.project({TILE_MAX_DIM*TILE_SIZE[0]/2.f, TILE_MAX_DIM*TILE_SIZE[1]/2.f, 0});
    //camera_offset = {};
}



} // namespace Magnum::Examples

MAGNUM_APPLICATION_MAIN(Magnum::Examples::app)

#ifdef _MSC_VER
#   include <cstdlib>
#   ifdef __clang__
#       pragma clang diagnostic ignored "-Wmissing-prototypes"
#       pragma clang diagnostic ignored "-Wmain"
#   endif

extern "C" int __stdcall WinMain(void*, void*, void*, int /* nCmdShow */) {
    return main(__argc, __argv);
}
#endif
