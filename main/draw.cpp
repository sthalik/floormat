#include "app.hpp"
#include "tile-defs.hpp"
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Trade/AbstractImporter.h>

namespace floormat {

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

    //update_window_scale(windowSize());
    {
        float dt = std::min(1.f/20, timeline.previousFrameDuration());
        update(dt);
    }

    _shader.set_tint({1, 1, 1, 1});
    draw_chunk(_chunk);
    draw_cursor_tile();
    display_menu();

    swapBuffers();
    redraw();
    timeline.nextFrame();
}

void app::draw_chunk(chunk& c)
{
    {
        int minx = 0, maxx = 0, miny = 0, maxy = 0;
        auto fn = [&](int x, int y) {
          const auto pos = pixel_to_tile({(float)x, (float)y}) / Vector2{TILE_MAX_DIM, TILE_MAX_DIM};
          minx = std::min(minx, (int)std::floor(pos[0]));
          maxx = std::max(maxx, (int)(pos[0]));
          miny = std::min(miny, (int)std::floor(pos[1]));
          maxy = std::max(maxy, (int)(pos[1]));
        };
        const auto sz = windowSize();
        const auto x = sz[0], y = sz[1];
        fn(0, 0);
        fn(x, 0);
        fn(0, y);
        fn(x, y);

        printf("%d %d -> %d %d\n", minx, miny, maxx, maxy);
        fflush(stdout);
        printf(""); // put breakpoint here
    }

    _shader.set_tint({1, 1, 1, 1});
    _floor_mesh.draw(_shader, c);
    _wall_mesh.draw(_shader, c);
}

void app::draw_wireframe_quad(local_coords pt)
{
    constexpr float LINE_WIDTH = 1;

    constexpr auto X = TILE_SIZE[0], Y = TILE_SIZE[1];
    const Vector3 center {X*pt.x, Y*pt.y, 0};
    _shader.set_tint({1, 0, 0, 1});
    _wireframe_quad.draw(_shader, {center, {TILE_SIZE[0], TILE_SIZE[1]}, LINE_WIDTH});
}

void app::draw_wireframe_box(local_coords pt)
{
    constexpr float LINE_WIDTH = 1.5;

    constexpr auto X = TILE_SIZE[0], Y = TILE_SIZE[1];
    constexpr Vector3 size{TILE_SIZE[0], TILE_SIZE[1], TILE_SIZE[2]*1.5f};
    const Vector3 center1{X*pt.x, Y*pt.y, 0};
    _shader.set_tint({0, 1, 0, 1});
    _wireframe_box.draw(_shader, {center1, size, LINE_WIDTH});
}

} // namespace floormat

MAGNUM_APPLICATION_MAIN(floormat::app)

#ifdef _MSC_VER
#include <cstdlib> // for __arg{c,v}
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wmain"
#endif
extern "C" int __stdcall WinMain(void*, void*, void*, int);

extern "C" int __stdcall WinMain(void*, void*, void*, int)
{
    return main(__argc, __argv);
}
#ifdef __clang__
#    pragma clang diagnostic pop
#endif
#endif
