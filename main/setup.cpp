#include "main-impl.hpp"
#include "compat/fpu.hpp"
#include <algorithm>
#include <Corrade/Containers/StringView.h>

namespace floormat {

main_impl::main_impl(floormat_app& app, fm_settings&& se, int& fake_argc) noexcept :
    Platform::Sdl2Application{Arguments{fake_argc, nullptr},
                              make_conf(se), make_gl_conf(se)},
    s{std::move(se)}, app{app}
{
    if (s.vsync)
    {
        (void)setSwapInterval(1);
        if (const auto list = GL::Context::current().extensionStrings();
            std::find(list.cbegin(), list.cend(), "EXT_swap_control_tear") != list.cend())
            (void)setSwapInterval(-1);
    }
    else
        (void)setSwapInterval(0);
    set_fp_mask();
    fm_assert(framebufferSize() == windowSize());
    _clickable_scenery.reserve(128);
    timeline.start();
}

auto main_impl::make_window_flags(const fm_settings& s) -> Configuration::WindowFlags
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

auto main_impl::make_conf(const fm_settings& s) -> Configuration
{
    switch (s.log_level)
    {
    default:
        SDL_setenv("MAGNUM_LOG_LEVEL", "normal", 1);
        break;
    case fm_log_level::quiet:
        SDL_setenv("MAGNUM_LOG_LEVEL", "quiet", 1);
        break;
    case fm_log_level::verbose:
        SDL_setenv("MAGNUM_LOG_LEVEL", "verbose", 1);
        break;
    }
    return Configuration{}
        .setTitle(s.title)
        .setSize(s.resolution, Vector2(1, 1))
        .setWindowFlags(make_window_flags(s));
}

auto main_impl::make_gl_conf(const fm_settings& s) -> GLConfiguration
{
    GLConfiguration::Flags flags{};
    using f = GLConfiguration::Flag;
    flags |= f::ForwardCompatible;
    if (s.gpu_debug >= fm_gpu_debug::on)
        flags |= f::Debug | f::GpuValidation;
    if (s.gpu_debug >= fm_gpu_debug::robust)
        flags |= f::RobustAccess | f::ResetIsolation;
    else if (s.gpu_debug <= fm_gpu_debug::no_error)
        flags |= f::NoError;
    return GLConfiguration{}.setFlags(flags);
}

static int get_window_refresh_rate(SDL_Window* window)
{
    fm_assert(window != nullptr);
    if (int index = SDL_GetWindowDisplayIndex(window); index < 0)
        fm_warn_once("SDL_GetWindowDisplayIndex: %s", SDL_GetError());
    else if (SDL_DisplayMode dpymode; SDL_GetCurrentDisplayMode(index, &dpymode) < 0)
        fm_warn_once("SDL_GetCurrentDisplayMode: %s", SDL_GetError());
    else
        return std::max(30, dpymode.refresh_rate);
    return 30;
}

void main_impl::update_window_state()
{
    const auto flags = (SDL_WindowFlags)SDL_GetWindowFlags(window());

    dt_expected.do_sleep = true;
    dt_expected.jitter = 0;
    if (flags & SDL_WINDOW_HIDDEN)
        dt_expected.value = 1;
    else if (int interval = std::abs(SDL_GL_GetSwapInterval()); s.vsync && interval > 0)
    {
        int hz = get_window_refresh_rate(window()) / interval;
        if (!(flags & SDL_WINDOW_INPUT_FOCUS))
            dt_expected.value = 2.f / hz;
        else
            dt_expected.value = std::floor((1.f/hz - 1e-3f) * 1e3f) * 1e-3f;
    }
    else
    {
        dt_expected.do_sleep = false;
        dt_expected.value = 1e-1f;
    }
}

} // namespace floormat
