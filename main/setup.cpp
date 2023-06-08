#include "main-impl.hpp"
#include "compat/fpu.hpp"
#include <algorithm>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/StringIterable.h>

namespace floormat {

main_impl::main_impl(floormat_app& app, fm_settings&& se, int& argc, char** argv) noexcept :
    Platform::Sdl2Application{Arguments{argc, argv},
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
        .setDepthBufferSize(0)
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
            dt_expected.value = std::floor((1.f/hz - 1e-3f) * 1e3f) * 1e-3f;
    }
    else
    {
        dt_expected.do_sleep = false;
        dt_expected.value = 1e-1f;
    }
}

auto main_impl::meshes() noexcept -> struct meshes
{
    return { _floor_mesh, _wall_mesh, _anim_mesh, };
};

struct world& main_impl::reset_world() noexcept
{
    return reset_world(floormat::world{});
}

struct world& main_impl::reset_world(struct world&& w) noexcept
{
    _clickable_scenery.clear();
    _world = std::move(w);
    return _world;
}

} // namespace floormat
