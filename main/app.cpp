#include <cstddef>
#include "compat/sysexits.hpp"
#include "app.hpp"
#include "compat/fpu.hpp"
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/DebugStl.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Context.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <SDL_events.h>
#include <SDL_video.h>

namespace floormat {

int app::run_from_argv(int argc, char** argv)
{
    Corrade::Utility::Arguments args{};
    app_settings opts;
    args.addSkippedPrefix("magnum")
        .addOption("vsync", opts.vsync ? "1" : "0")
        .parse(argc, argv);
    opts.vsync = args.value<bool>("vsync");
    app x{{argc, argv}, std::move(opts)}; // NOLINT(performance-move-const-arg)
    return x.exec();
}

void app::usage(const Utility::Arguments& args)
{
    Error{Error::Flag::NoNewlineAtTheEnd} << args.usage();
    std::exit(EX_USAGE); // NOLINT(concurrency-mt-unsafe)
}

app::app(const Arguments& arguments, app_settings opts):
      Platform::Application{
          arguments,
          Configuration{}
              .setTitle("Test")
              .setSize({1024, 768}, dpi_policy::Physical)
              .setWindowFlags(Configuration::WindowFlag::Resizable),
          GLConfiguration{},
      },
      _settings{opts}
{
    SDL_MaximizeWindow(window());

    if (opts.vsync)
    {
        (void)setSwapInterval(1);
        if (const auto list = GL::Context::current().extensionStrings();
            std::find(list.cbegin(), list.cend(), "EXT_swap_control_tear") != list.cbegin())
            (void)setSwapInterval(-1);
    }
    else
        setSwapInterval(0);

    set_fp_mask();
    reset_camera_offset();

#if 1
    fm_assert(framebufferSize() == windowSize());
    _imgui = ImGuiIntegration::Context(Vector2{windowSize()}, windowSize(), framebufferSize());
    recalc_viewport(windowSize());
#else
    _msaa_color_texture.setStorage(1, GL::TextureFormat::RGBA8, windowSize());
    _framebuffer = GL::Framebuffer{GL::defaultFramebuffer.viewport()};
    _framebuffer.attachTexture(GL::Framebuffer::ColorAttachment{0}, _msaa_color_texture);
#endif
    //_framebuffer.attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, depthStencil);

    setMinimalLoopPeriod(5);
    {
        auto c = _world[chunk_coords{0, 0}];
        make_test_chunk(*c);
    }
    timeline.start();
}

void app::recalc_viewport(Vector2i size)
{
    _shader.set_scale(Vector2(size));
    _imgui.relayout(Vector2{size}, size, size);

    _cursor_pixel = std::nullopt;
    recalc_cursor_tile();

    GL::defaultFramebuffer.setViewport({{}, size });
    _framebuffer.detach(GL::Framebuffer::ColorAttachment{0});
    _msaa_color_texture = GL::MultisampleTexture2D{};
    _msaa_color_texture.setStorage(1, GL::TextureFormat::RGBA8, size);
    _framebuffer.setViewport({{}, size });
    _framebuffer.attachTexture(GL::Framebuffer::ColorAttachment{0}, _msaa_color_texture);
}

void app::viewportEvent(Platform::Sdl2Application::ViewportEvent& event)
{
    fm_assert(event.framebufferSize() == event.windowSize());
    recalc_viewport(event.windowSize());
}

void app::mousePressEvent(Platform::Sdl2Application::MouseEvent& event)
{
    if (_imgui.handleMousePressEvent(event))
        return event.setAccepted();
    else if (_cursor_tile)
    {
        const auto& tile = *_cursor_tile;
        do_mouse_click(tile, (int)event.button());
    }
}

void app::mouseReleaseEvent(Platform::Sdl2Application::MouseEvent& event)
{
    if (_imgui.handleMouseReleaseEvent(event))
        return event.setAccepted();
    do_mouse_release((int)event.button());
}

void app::mouseMoveEvent(Platform::Sdl2Application::MouseMoveEvent& event)
{
    _cursor_in_imgui = _imgui.handleMouseMoveEvent(event);
    if (_cursor_in_imgui)
        _cursor_pixel = std::nullopt;
    else
        _cursor_pixel = event.position();
    recalc_cursor_tile();
    if (_cursor_tile)
        do_mouse_move(*_cursor_tile);
}

void app::mouseScrollEvent(Platform::Sdl2Application::MouseScrollEvent& event)
{
    if (_imgui.handleMouseScrollEvent(event))
        return event.setAccepted();
}

void app::textInputEvent(Platform::Sdl2Application::TextInputEvent& event)
{
    if (_imgui.handleTextInputEvent(event))
    {
        keys = {};
        event.setAccepted();
    }
}

void app::keyPressEvent(Platform::Sdl2Application::KeyEvent& event)
{
    if (_imgui.handleKeyPressEvent(event))
    {
        keys = {};
        return event.setAccepted();
    }
    do_key(event.key(), event.modifiers(), true, event.isRepeated());
}

void app::keyReleaseEvent(Platform::Sdl2Application::KeyEvent& event)
{
    if (_imgui.handleKeyReleaseEvent(event))
    {
        keys = {};
        return event.setAccepted();
    }
    do_key(event.key(), event.modifiers(), false, false);
}

void app::anyEvent(SDL_Event& event)
{
    if (event.type == SDL_WINDOWEVENT)
        switch (event.window.event)
        {
        case SDL_WINDOWEVENT_FOCUS_LOST:
            return event_focus_out();
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            return event_focus_in();
        case SDL_WINDOWEVENT_LEAVE:
            return event_mouse_leave();
        case SDL_WINDOWEVENT_ENTER:
            return event_mouse_enter();
        default:
            std::fputs("", stdout); break; // put breakpoint here
        }
}

void app::event_focus_out()
{
    _cursor_pixel = std::nullopt;
    recalc_cursor_tile();
}

void app::event_focus_in()
{
}

void app::event_mouse_leave()
{
    _cursor_pixel = std::nullopt;
    recalc_cursor_tile();
}

void app::event_mouse_enter()
{
}

} // namespace floormat

int main(int argc, char** argv)
{
    return floormat::app::run_from_argv(argc, argv);
}

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
