#include <cstddef>
#include "compat/sysexits.hpp"
#include "main.hpp"
#include "compat/fpu.hpp"
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/DebugStl.h>
#include <Magnum/GL/DefaultFramebuffer.h>

#ifdef FM_MSAA
#include <Magnum/GL/RenderbufferFormat.h>
#endif

namespace floormat {

int floormat::run_from_argv(int argc, char** argv)
{
    Corrade::Utility::Arguments args{};
    app_settings opts;
    args.addSkippedPrefix("magnum")
        .addOption("vsync", opts.vsync ? "1" : "0")
        .parse(argc, argv);
    opts.vsync = args.value<bool>("vsync");
    floormat x{{argc, argv}, std::move(opts)}; // NOLINT(performance-move-const-arg)
    return x.exec();
}

void floormat::usage(const Utility::Arguments& args)
{
    Error{Error::Flag::NoNewlineAtTheEnd} << args.usage();
    std::exit(EX_USAGE); // NOLINT(concurrency-mt-unsafe)
}

floormat::floormat(const Arguments& arguments, app_settings opts):
      Platform::Sdl2Application{
          arguments,
          Configuration{}
              .setTitle("Test")
              .setSize({1024, 768}, dpi_policy::Physical)
              .setWindowFlags(Configuration::WindowFlag::Resizable),
          GLConfiguration{}
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

    fm_assert(framebufferSize() == windowSize());
    recalc_viewport(windowSize());

    setMinimalLoopPeriod(5);
    {
        auto c = _world[chunk_coords{0, 0}];
        make_test_chunk(*c);
    }
    timeline.start();
}

void floormat::recalc_viewport(Vector2i size)
{
    _shader.set_scale(Vector2(size));
    init_imgui(size);
    _cursor_pixel = std::nullopt;
    recalc_cursor_tile();

    GL::defaultFramebuffer.setViewport({{}, size });
#ifdef FM_MSAA
    _msaa_framebuffer.detach(GL::Framebuffer::ColorAttachment{0});
    _msaa_renderbuffer = Magnum::GL::Renderbuffer{};
    _msaa_renderbuffer.setStorageMultisample(msaa_samples, GL::RenderbufferFormat::RGBA8, size);
    _msaa_framebuffer.setViewport({{}, size });
    _msaa_framebuffer.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, _msaa_renderbuffer);
#endif
}

} // namespace floormat

int main(int argc, char** argv)
{
    return floormat::floormat::run_from_argv(argc, argv);
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
