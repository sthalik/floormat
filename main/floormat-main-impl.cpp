#include "floormat-main-impl.hpp"
#include "floormat.hpp"
#include "floormat-app.hpp"
#include "compat/assert.hpp"
#include "compat/fpu.hpp"

namespace floormat {

floormat_main::floormat_main() noexcept = default;
floormat_main::~floormat_main() noexcept = default;

static const char* const fm_fake_argv[] = { "floormat", nullptr };

auto main_impl::make_window_flags(const fm_options& s) -> Configuration::WindowFlags
{
    using flag = Configuration::WindowFlag;
    Configuration::WindowFlags flags{};
    if (s.resizable)
        flags |= flag::Resizable;
    if (s.fullscreen)
        flags |= flag::Fullscreen;
    if (s.fullscreen_desktop)
        flags |= flag::FullscreenDesktop;
    if (s.borderless)
        flags |= flag::Borderless;
    if (s.maximized)
        flags |= flag::Maximized;
    return flags;
}

void main_impl::recalc_viewport(Vector2i size)
{
    GL::defaultFramebuffer.setViewport({{}, size });
#ifdef FM_MSAA
    _msaa_framebuffer.detach(GL::Framebuffer::ColorAttachment{0});
    _msaa_renderbuffer = Magnum::GL::Renderbuffer{};
    _msaa_renderbuffer.setStorageMultisample(s.msaa_samples, GL::RenderbufferFormat::RGBA8, size);
    _msaa_framebuffer.setViewport({{}, size });
    _msaa_framebuffer.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, _msaa_renderbuffer);
#endif
    _shader.set_scale(Vector2(size));
    app.on_viewport_event(size);
    setMinimalLoopPeriod(5);
}

auto main_impl::make_conf(const fm_options& s) -> Configuration
{
    return Configuration{}
           .setTitle(s.title)
           .setSize(s.resolution)
           .setWindowFlags(make_window_flags(s));
}

main_impl::main_impl(floormat_app& app, const fm_options& s) :
    Platform::Sdl2Application{Arguments{fake_argc, fm_fake_argv},
                              make_conf(s), make_gl_conf(s)},
    app{app}, s{s}
{
    switch (s.vsync)
    {
    case fm_tristate::on:
        (void)setSwapInterval(1);
        if (const auto list = GL::Context::current().extensionStrings();
            std::find(list.cbegin(), list.cend(), "EXT_swap_control_tear") != list.cbegin())
            (void)setSwapInterval(-1);
        break;
    case fm_tristate::off:
        setSwapInterval(0);
        break;
    default: break;
    }
    set_fp_mask();
    fm_assert(framebufferSize() == windowSize());
    recalc_viewport(windowSize());
    timeline.start();
}

main_impl::~main_impl() = default;

void main_impl::drawEvent()
{
    if (const float dt = timeline.previousFrameDuration(); dt > 0)
    {
        constexpr float RC = 0.1f;
        const float alpha = dt/(dt + RC);

        _frame_time = _frame_time*(1-alpha) + alpha*dt;
    }
    else
        swapBuffers();
        timeline.nextFrame();

    const auto dt = std::clamp((double)timeline.previousFrameDuration(), 1e-6, 1e-1);
    app.update(dt);

    _shader.set_tint({1, 1, 1, 1});
}

floormat_main* floormat_main::create(floormat_app& app, const fm_options& options)
{
    auto* ret = new main_impl(app, options);
    fm_assert(ret);
    return ret;
}

} // namespace floormat
