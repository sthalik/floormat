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

    update_window_scale(windowSize());
    {
        float dt = timeline.previousFrameDuration();
        update(dt);
    }

    draw_chunk(_chunk);
    draw_wireframe();

    swapBuffers();
    redraw();
    timeline.nextFrame();
}

void app::draw_chunk(chunk& c)
{
    _shader.set_tint({1, 1, 1, 1});
    _floor_mesh.draw(_shader, c);
    _wall_mesh.draw(_shader, c);
}

void app::draw_wireframe()
{
    _shader.set_tint({1.f, 0, 0, 1.f});
    _wireframe_quad.draw(_shader, {{}, {TILE_SIZE[0], TILE_SIZE[1]}});
}

} // namespace Magnum::Examples

MAGNUM_APPLICATION_MAIN(Magnum::Examples::app)

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
