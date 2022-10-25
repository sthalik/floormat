#include "floormat-main-impl.hpp"
#include "floormat.hpp"
#include "floormat-app.hpp"
#include "compat/assert.hpp"
#include "compat/fpu.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include "src/camera-offset.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>
#include <cstdlib>

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
        setMinimalLoopPeriod(0);
        break;
    case fm_tristate::off:
        setSwapInterval(0);
        break;
    default: break;
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

void main_impl::drawEvent()
{
    if (const float dt = timeline.previousFrameDuration(); dt > 0)
    {
        constexpr float RC = 0.1f;
        const float alpha = dt/(dt + RC);

        _frame_time = _frame_time*(1-alpha) + alpha*dt;
    }
    else
    {
        swapBuffers();
        timeline.nextFrame();
    }

    {
        float dt_max;
        unsigned min_loop_period;

        if (const auto flags = SDL_GetWindowFlags(window());
            flags & SDL_WINDOW_HIDDEN)
        {
            dt_max = 1 + 1e-3f;
            min_loop_period = 1000;
        }
        else if (!(flags & (SDL_WINDOW_INPUT_FOCUS|SDL_WINDOW_MOUSE_FOCUS)))
        {
            dt_max = 1.f/10;
            min_loop_period = 1000/12;
        }
        else
        {
            dt_max = 1e-1f;
            min_loop_period = 0;
        }

        const float dt = std::clamp(timeline.previousFrameDuration(), 1e-6f, dt_max);
        setMinimalLoopPeriod(min_loop_period);

        app.update(dt);
    }

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
