#include "main-impl.hpp"
#include "floormat/app.hpp"
#include "src/camera-offset.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>
#include <algorithm>
#include <thread>

//#define FM_SKIP_MSAA

namespace floormat {

void main_impl::recalc_viewport(Vector2i size) noexcept
{
    update_window_state();
    _shader.set_scale(Vector2{size});
    GL::defaultFramebuffer.setViewport({{}, size });
    if (_msaa_color.id())
        _msaa_framebuffer.detach(GL::Framebuffer::ColorAttachment{0});
    if (_msaa_depth.id())
        _msaa_framebuffer.detach(GL::Framebuffer::BufferAttachment::DepthStencil);
    if (_msaa_framebuffer.id())
        _msaa_framebuffer.setViewport({{}, size});
    else
        _msaa_framebuffer = GL::Framebuffer{{{}, size}};
    // --- color ---
    _msaa_color = Magnum::GL::Renderbuffer{};
    const int samples = std::min(_msaa_color.maxSamples(), (int)s.msaa_samples);
    _msaa_color.setStorageMultisample(samples, GL::RenderbufferFormat::RGBA8, size);
    _msaa_framebuffer.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, _msaa_color);
    // --- depth ---
    _msaa_depth = Magnum::GL::Renderbuffer{};
    _msaa_depth.setStorageMultisample(samples, GL::RenderbufferFormat::DepthStencil, size);
    _msaa_framebuffer.attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, _msaa_depth);
    // -- done ---
    app.on_viewport_event(size);
}

global_coords main_impl::pixel_to_tile(Vector2d position) const noexcept
{
    constexpr Vector2d pixel_size(TILE_SIZE2);
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
    const auto sz = windowSize();

    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            if (const chunk_coords c = {x, y}; !_world.contains(c))
                app.maybe_initialize_chunk(c, _world[c]);
            const chunk_coords c{x, y};
            const with_shifted_camera_offset o{_shader, c};
            if (check_chunk_visible(_shader.camera_offset(), sz))
                _floor_mesh.draw(_shader, _world[c]);
        }

    //GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            const chunk_coords c{x, y};
            const with_shifted_camera_offset o{_shader, c};
            if (check_chunk_visible(_shader.camera_offset(), sz))
                _wall_mesh.draw(_shader, _world[c]);
        }
}

bool main_impl::check_chunk_visible(const Vector2d& offset, const Vector2i& size) noexcept
{
    constexpr Vector3d len = dTILE_SIZE * TILE_MAX_DIM20d;
    enum : std::size_t { x, y, };
    constexpr Vector2d p00 = tile_shader::project(Vector3d(0, 0, 0)),
                       p10 = tile_shader::project(Vector3d(len[x], 0, 0)),
                       p01 = tile_shader::project(Vector3d(0, len[y], 0)),
                       p11 = tile_shader::project(Vector3d(len[x], len[y], 0));
    constexpr auto xx = std::minmax({ p00[x], p10[x], p01[x], p11[x], }), yy = std::minmax({ p00[y], p10[y], p01[y], p11[y], });
    constexpr auto minx = xx.first, maxx = xx.second, miny = yy.first, maxy = yy.second;
    constexpr int W = (int)(maxx - minx + .5 + 1e-16), H = (int)(maxy - miny + .5 + 1e-16);
    const auto X = (int)(minx + (offset[x] + size[x])*.5), Y = (int)(miny + (offset[y] + size[y])*.5);
    return X + W > 0 && X < size[x] && Y + H > 0 && Y < size[y];
}

void main_impl::drawEvent()
{
    float dt = timeline.previousFrameDuration();
    if (dt > 0)
    {
        const float RC1 = dt_expected.do_sleep ? 1.f : 1.f/4,
                    RC2 = 1.f/8;
        const float alpha1 = dt/(dt + RC1);
        const float alpha2 = dt/(dt + RC2);

        _frame_time1 = _frame_time1*(1-alpha1) + alpha1*dt;
        _frame_time2 = _frame_time1*(1-alpha2) + alpha2*dt;
        constexpr float max_deviation = 10 * 1e-3f;
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
        using fc = GL::FramebufferClear;
        constexpr auto mask = fc::Color | fc::Depth | fc::Stencil;
        GL::defaultFramebuffer.clear(mask);
#ifndef FM_SKIP_MSAA
        _msaa_framebuffer.clear(mask);
        _msaa_framebuffer.bind();
#endif
        draw_world();
        app.draw_msaa();
#ifndef FM_SKIP_MSAA
        GL::defaultFramebuffer.bind();
        GL::Framebuffer::blit(_msaa_framebuffer, GL::defaultFramebuffer, {{}, windowSize()}, GL::FramebufferBlit{(unsigned)mask});
#endif
    }

    app.draw();

    swapBuffers();
    redraw();

    if (dt_expected.do_sleep)
    {
        constexpr float ε = 1e-3f;
        const float Δt൦ = timeline.currentFrameDuration(), sleep_secs = dt_expected.value - Δt൦ - dt_expected.jitter;
        if (sleep_secs > ε)
            std::this_thread::sleep_for(std::chrono::nanoseconds((long long)(sleep_secs * 1e9f)));
        //fm_debug("jitter:%.1f sleep:%.0f", dt_expected.jitter*1000, sleep_secs*1000);
        const float Δt = timeline.currentFrameDuration() - dt_expected.value;
        constexpr float α = .1f;
        dt_expected.jitter = std::fmax(dt_expected.jitter + Δt * α,
                                       dt_expected.jitter * (1-α) + Δt * α);
        dt_expected.jitter = std::copysign(std::fmin(dt_expected.value, std::fabs(dt_expected.jitter)), dt_expected.jitter);
    }
    else
        dt_expected.jitter = 0;
    timeline.nextFrame();
}

} // namespace floormat
