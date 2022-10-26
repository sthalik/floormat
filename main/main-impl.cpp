#include "main-impl.hpp"
#include "floormat/app.hpp"
#include "compat/assert.hpp"
#include "compat/fpu.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include "src/camera-offset.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>
#include <chrono>
#include <thread>

//#define FM_SKIP_MSAA

namespace floormat {

floormat_main::floormat_main() noexcept = default;
floormat_main::~floormat_main() noexcept = default;
main_impl::~main_impl() noexcept = default;

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
        .setSize(s.resolution)
        .setWindowFlags(make_window_flags(s));
}

auto main_impl::make_gl_conf(const fm_settings& s) -> GLConfiguration
{
    GLConfiguration::Flags flags{};
    using f = GLConfiguration::Flag;
    if (s.gpu_debug >= fm_gpu_debug::on)
        flags |= f::Debug | f::GpuValidation;
    if (s.gpu_debug >= fm_gpu_debug::robust)
        flags |= f::RobustAccess;
    else if (s.gpu_debug <= fm_gpu_debug::no_error)
        flags |= f::NoError;
    return GLConfiguration{}.setFlags(flags);
}

void main_impl::recalc_viewport(Vector2i size) noexcept
{
    update_window_state();
    GL::defaultFramebuffer.setViewport({{}, size });
    _msaa_framebuffer.detach(GL::Framebuffer::ColorAttachment{0});
    _msaa_renderbuffer = Magnum::GL::Renderbuffer{};
    _msaa_renderbuffer.setStorageMultisample(s.msaa_samples, GL::RenderbufferFormat::RGBA8, size);
    _msaa_framebuffer.setViewport({{}, size });
    _msaa_framebuffer.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, _msaa_renderbuffer);
    _shader.set_scale(Vector2{size});
    app.on_viewport_event(size);
}

main_impl::main_impl(floormat_app& app, fm_settings&& s, int& fake_argc) noexcept :
    Platform::Sdl2Application{Arguments{fake_argc, nullptr},
                              make_conf(s), make_gl_conf(s)},
    s{std::move(s)}, app{app}
{
    switch (s.vsync) // NOLINT(bugprone-use-after-move)
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
    default:
        break;
    }
    set_fp_mask();
    fm_assert(framebufferSize() == windowSize());
    timeline.start();
}

global_coords main_impl::pixel_to_tile(Vector2d position) const noexcept
{
    constexpr Vector2d pixel_size{dTILE_SIZE[0], dTILE_SIZE[1]};
    constexpr Vector2d half{.5, .5};
    const Vector2d px = position - Vector2d{windowSize()}*.5 - _shader.camera_offset()*.5;
    const Vector2d vec = tile_shader::unproject(px) / pixel_size + half;
    const auto x = (std::int32_t)std::floor(vec[0]), y = (std::int32_t)std::floor(vec[1]);
    return { x, y };
}

auto main_impl::get_draw_bounds() const noexcept -> draw_bounds
{

    using limits = std::numeric_limits<std::int16_t>;
    auto x0 = limits::max(), x1 = limits::min(), y0 = limits::max(), y1 = limits::min();

    for (const auto win = Vector2d(windowSize());
        auto p : {pixel_to_tile(Vector2d{0, 0}).chunk(),
                  pixel_to_tile(Vector2d{win[0]-1, 0}).chunk(),
                  pixel_to_tile(Vector2d{0, win[1]-1}).chunk(),
                  pixel_to_tile(Vector2d{win[0]-1, win[1]-1}).chunk()})
    {
        x0 = std::min(x0, p.x);
        x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y);
        y1 = std::max(y1, p.y);
    }
    return {x0, x1, y0, y1};
}

void main_impl::draw_world() noexcept
{
    auto [minx, maxx, miny, maxy] = get_draw_bounds();

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            if (const chunk_coords c = {x, y}; !_world.contains(c))
                app.maybe_initialize_chunk(c, _world[c]);
            const chunk_coords c{x, y};
            const with_shifted_camera_offset o{_shader, c};
            _floor_mesh.draw(_shader, _world[c]);
        }

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            const chunk_coords c{x, y};
            const with_shifted_camera_offset o{_shader, c};
            _wall_mesh.draw(_shader, _world[c]);
        }
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
    else if (!(flags & SDL_WINDOW_INPUT_FOCUS))
        dt_expected.value = 1.f / 30;
    else if (int interval = std::abs(SDL_GL_GetSwapInterval());
             s.vsync >= fm_tristate::maybe && interval > 0)
        dt_expected.value = 0.5f / (get_window_refresh_rate(window()));
#if 1
    else if (!(flags & SDL_WINDOW_MOUSE_FOCUS))
        dt_expected.value = 1.f / 60;
#endif
    else
    {
        dt_expected.do_sleep = false;
        dt_expected.value = 1e-1f;
    }
}

void main_impl::drawEvent()
{
    float dt = timeline.previousFrameDuration();
    if (dt > 0)
    {
        const float RC1 = dt_expected.do_sleep ? 1.f : 1.f/15,
                    RC2 = dt_expected.do_sleep ? 1.f/15 : 1.f/60;
        const float alpha1 = dt/(dt + RC1);
        const float alpha2 = dt/(dt + RC2);

        _frame_time1 = _frame_time1*(1-alpha1) + alpha1*dt;
        _frame_time2 = _frame_time1*(1-alpha2) + alpha2*dt;
        constexpr float max_deviation = 5 * 1e-3f;
        if (std::fabs(_frame_time1 - _frame_time2) > max_deviation)
            _frame_time1 = _frame_time2;
    }
    else
    {
        swapBuffers();
        timeline.nextFrame();
    }

    dt = std::clamp(dt, 1e-5f, std::fmaxf(1e-1f, dt_expected.value));

    app.update(dt);
    _shader.set_tint({1, 1, 1, 1});

    {
        GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);
#ifndef FM_SKIP_MSAA
        _msaa_framebuffer.clear(GL::FramebufferClear::Color);
        _msaa_framebuffer.bind();
#endif
        draw_world();
        app.draw_msaa();
#ifndef FM_SKIP_MSAA
        GL::defaultFramebuffer.bind();
        GL::Framebuffer::blit(_msaa_framebuffer, GL::defaultFramebuffer, {{}, windowSize()}, GL::FramebufferBlit::Color);
#endif
    }

    app.draw();

    swapBuffers();
    redraw();

    if (dt_expected.do_sleep)
    {
        constexpr float eps = 1e-3f;
        const float Δt൦ = timeline.currentFrameDuration(), sleep_secs = dt_expected.value - Δt൦ - dt_expected.jitter;
        if (sleep_secs > eps)
            std::this_thread::sleep_for(std::chrono::nanoseconds((long long)(sleep_secs * 1e9f)));
        //fm_debug("jitter:%.1f sleep:%.0f", dt_expected.jitter*1000, sleep_secs*1000);
        const float Δt = timeline.currentFrameDuration() - dt_expected.value;
        constexpr float α = .1f;
        dt_expected.jitter = std::fmax(dt_expected.jitter + Δt * α,
                                       dt_expected.jitter * (1-α) + Δt * α);
        dt_expected.jitter = std::copysignf(std::fminf(dt_expected.value, std::fabsf(dt_expected.jitter)), dt_expected.jitter);
    }
    else
        dt_expected.jitter = 0;
    timeline.nextFrame();
}

void main_impl::quit(int status) { Platform::Sdl2Application::exit(status); }
struct world& main_impl::world() noexcept { return _world; }
SDL_Window* main_impl::window() noexcept { return Sdl2Application::window(); }
fm_settings& main_impl::settings() noexcept { return s; }
const fm_settings& main_impl::settings() const noexcept { return s; }
Vector2i main_impl::window_size() const noexcept { return windowSize(); }
tile_shader& main_impl::shader() noexcept { return _shader; }
const tile_shader& main_impl::shader() const noexcept { return _shader; }
bool main_impl::is_text_input_active() const noexcept { return const_cast<main_impl&>(*this).isTextInputActive(); }
void main_impl::start_text_input() noexcept { startTextInput(); }
void main_impl::stop_text_input() noexcept { stopTextInput(); }

int main_impl::exec()
{
    recalc_viewport(windowSize());
    return Sdl2Application::exec();
}

floormat_main* floormat_main::create(floormat_app& app, fm_settings&& options)
{
    int fake_argc = 0;
    auto* ret = new main_impl(app, std::move(options), fake_argc);
    fm_assert(ret);
    return ret;
}

} // namespace floormat
