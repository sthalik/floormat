#include "main-impl.hpp"
#include <algorithm>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/StringIterable.h>

namespace floormat {

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
    return Configuration{}
        .setTitle(s.title ? (StringView)s.title : "floormat editor"_s)
        .setSize(s.resolution)
        .setWindowFlags(make_window_flags(s));
}

auto main_impl::make_gl_conf(const fm_settings&) -> GLConfiguration
{
    GLConfiguration::Flags flags{};
    flags |= GLConfiguration::Flag::ForwardCompatible;
    return GLConfiguration{}
        .setFlags(flags)
#ifdef FM_USE_DEPTH32
        .setDepthBufferSize(0)
#endif
        .setColorBufferSize({8, 8, 8, 0})
        .setStencilBufferSize(0);
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
            dt_expected.value = 1.f/hz;
    }
    else
    {
        dt_expected.do_sleep = false;
        dt_expected.value = 1e-1f;
    }
}

auto main_impl::meshes() noexcept -> struct meshes
{
    return { _ground_mesh, _wall_mesh, _anim_mesh, };
};

class world& main_impl::reset_world() noexcept
{
    return reset_world(floormat::world{});
}

} // namespace floormat
