#include "main-impl.hpp"
#include "src/tile-constants.hpp"
#include "floormat/app.hpp"
#include "src/camera-offset.hpp"
#include "src/anim-atlas.hpp"
#include "main/clickable.hpp"
#include "src/light.hpp"
#include "src/log.hpp"
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Functions.h>
#include <thread>
#include <algorithm>

namespace floormat {

void main_impl::recalc_viewport(Vector2i fb_size, Vector2i win_size) noexcept
{
    _dpi_scale = dpiScaling();
    _virtual_scale = Vector2(fb_size) / Vector2(win_size);
    update_window_state();
    _shader.set_scale(Vector2{fb_size});

    GL::Renderer::setDepthMask(true);

#ifdef FM_USE_DEPTH32
    {
        framebuffer.fb = GL::Framebuffer{{ {}, fb_size }};

        framebuffer.color = GL::Texture2D{};
        framebuffer.color.setStorage(1, GL::TextureFormat::RGBA8, fb_size);
        framebuffer.depth = GL::Renderbuffer{};
        framebuffer.depth.setStorage(GL::RenderbufferFormat::DepthComponent32F, fb_size);

        framebuffer.fb.attachTexture(GL::Framebuffer::ColorAttachment{0}, framebuffer.color, 0);
        framebuffer.fb.attachRenderbuffer(GL::Framebuffer::BufferAttachment::Depth, framebuffer.depth);
        framebuffer.fb.clearColor(0, Color4{0.f, 0.f, 0.f, 1.f});
        framebuffer.fb.clearDepth(0);

        framebuffer.fb.bind();
    }
#else
    GL::defaultFramebuffer.setViewport({{}, fb_size });
    GL::defaultFramebuffer.clearColor(Color4{0.f, 0.f, 0.f, 1.f});
    GL::defaultFramebuffer.clearDepthStencil(0, 0);
    GL::defaultFramebuffer.bind();
#endif

    // -- state ---
    glEnable(GL_LINE_SMOOTH);
    using BlendEquation   = GL::Renderer::BlendEquation;
    using BlendFunction   = GL::Renderer::BlendFunction;
    using DepthFunction   = GL::Renderer::DepthFunction;
    using ProvokingVertex = GL::Renderer::ProvokingVertex;
    using Feature         = GL::Renderer::Feature;
    GL::Renderer::setBlendEquation(BlendEquation::Add, BlendEquation::Add);
    GL::Renderer::setBlendFunction(BlendFunction::SourceAlpha, BlendFunction::OneMinusSourceAlpha);
    GL::Renderer::disable(Feature::FaceCulling);
    GL::Renderer::disable(Feature::DepthTest);
    GL::Renderer::enable(Feature::Blending);
    GL::Renderer::enable(Feature::ScissorTest);
    GL::Renderer::enable(Feature::DepthClamp);
    GL::Renderer::setDepthFunction(DepthFunction::Greater);
    GL::Renderer::setScissor({{}, fb_size});
    GL::Renderer::setProvokingVertex(ProvokingVertex::FirstVertexConvention);

    // -- user--
    app.on_viewport_event(fb_size);

    fps_sample_timeline.start();
}

global_coords main_impl::pixel_to_tile(Vector2d position) const noexcept
{
    auto vec = pixel_to_tile_(position);
    auto vec_ = Math::floor(vec);
    return { (int32_t)vec_.x(), (int32_t)vec.y(), 0 };
}

Vector2d main_impl::pixel_to_tile_(Vector2d position) const noexcept
{
    constexpr Vector2d pixel_size(TILE_SIZE2);
    constexpr Vector2d half{.5, .5};
    const Vector2d px = position - Vector2d{window_size()}*.5 - _shader.camera_offset();
    return tile_shader::unproject(px*.5) / pixel_size + half;
}

auto main_impl::get_draw_bounds() const noexcept -> draw_bounds
{
    using limits = std::numeric_limits<int16_t>;
    auto x0 = limits::max(), x1 = limits::min(), y0 = limits::max(), y1 = limits::min();

    const auto win = Vector2d(window_size());

    chunk_coords list[] = {
        pixel_to_tile(Vector2d{0, 0}).chunk(),
        pixel_to_tile(Vector2d{win[0]-1, 0}).chunk(),
        pixel_to_tile(Vector2d{0, win[1]-1}).chunk(),
        pixel_to_tile(Vector2d{win[0]-1, win[1]-1}).chunk(),
    };

    auto center = pixel_to_tile(Vector2d{win[0]/2, win[1]/2}).chunk();

    for (auto p : list)
    {
        x0 = Math::min(x0, p.x);
        x1 = Math::max(x1, p.x);
        y0 = Math::min(y0, p.y);
        y1 = Math::max(y1, p.y);
    }
    // todo test this with the default viewport size using --magnum-dpi-scaling=1
    x0 -= 1; y0 -= 1; x1 += 1; y1 += 1;

    constexpr int16_t mx = tile_shader::max_screen_tiles.x()/(int16_t)2,
                      my = tile_shader::max_screen_tiles.y()/(int16_t)2;
    int16_t minx = center.x - mx + 1, maxx = center.x + mx,
            miny = center.y - my + 1, maxy = center.y + my;

    x0 = Math::clamp(x0, minx, maxx);
    x1 = Math::clamp(x1, minx, maxx);
    y0 = Math::clamp(y0, miny, maxy);
    y1 = Math::clamp(y1, miny, maxy);

    return {x0, x1, y0, y1};
}

void main_impl::draw_world() noexcept
{
    const auto [z_min, z_max, z_cur, only] = app.get_z_bounds();
    const auto [minx, maxx, miny, maxy] = get_draw_bounds();
    const auto sz = window_size();

    fm_debug_assert(1 + maxx - minx <= 8);
    fm_debug_assert(1 + maxy - miny <= 8);

    arrayResize(_clickable_scenery, 0);
#ifdef FM_USE_DEPTH32
        framebuffer.fb.clearDepth(0);
#else
        GL::defaultFramebuffer.clearDepth(0);
#endif
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::setDepthMask(true);

    for (int8_t z = z_max; z >= z_min; z--)
    {
        if (only && z != z_cur)
            _shader.set_tint({1, 1, 1, 0.75});
        else
            _shader.set_tint({1, 1, 1, 1});

        for (int16_t y = maxy; y >= miny; y--)
            for (int16_t x = maxx; x >= minx; x--)
            {
                const chunk_coords_ ch{x, y, z};
                auto* c_ = _world.at(ch);
                if (!c_)
                    continue;
                auto& c = *c_;
                bind();

                const with_shifted_camera_offset o{_shader, ch, {minx, miny}, {maxx, maxy}};
                if (check_chunk_visible(_shader.camera_offset(), sz))
                {
                    _ground_mesh.draw(_shader, c);
                    _wall_mesh.draw(_shader, c);
                }
            }
    }

    GL::Renderer::setDepthMask(false);

    for (int8_t z = z_min; z <= z_max; z++)
        for (int16_t y = miny; y <= maxy; y++)
            for (int16_t x = minx; x <= maxx; x++)
            {
                if (only && z != z_cur)
                    _shader.set_tint({1, 1, 1, 0.75});
                else
                    _shader.set_tint({1, 1, 1, 1});

                const chunk_coords_ pos{x, y, z};
                auto* c_ = _world.at(pos);
                if (!c_)
                    continue;
                auto& c = *c_;
                const with_shifted_camera_offset o{_shader, pos, {minx, miny}, {maxx, maxy}};
                if (check_chunk_visible(_shader.camera_offset(), sz))
                    _anim_mesh.draw(_shader, sz, c, _clickable_scenery, _do_render_vobjs);
            }

    _shader.set_tint({1, 1, 1, 1});
    GL::Renderer::setDepthMask(true);

    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
}

bool floormat_main::check_chunk_visible(const Vector2d& offset, const Vector2i& size) noexcept
{
    constexpr Vector3d len = dTILE_SIZE * TILE_MAX_DIM20d;
    enum : size_t { x, y, };
    constexpr Vector2d p00 = tile_shader::project(Vector3d(0, 0, 0)),
                       p10 = tile_shader::project(Vector3d(len[x], 0, 0)),
                       p01 = tile_shader::project(Vector3d(0, len[y], 0)),
                       p11 = tile_shader::project(Vector3d(len[x], len[y], 0));
    constexpr auto xx = std::minmax({ p00[x], p10[x], p01[x], p11[x], }),
                   yy = std::minmax({ p00[y], p10[y], p01[y], p11[y], });
    constexpr auto minx = xx.first, maxx = xx.second, miny = yy.first, maxy = yy.second;
    constexpr int W = (int)(maxx - minx + .5 + 1e-16), H = (int)(maxy - miny + .5 + 1e-16);
    const auto X = (int)(minx + (offset[x] + size[x])*.5), Y = (int)(miny + (offset[y] + size[y])*.5);
    return X + W > 0 && X < size[x] && Y + H > 0 && Y < size[y];
}

#ifndef FM_NO_DEBUG
static size_t good_frames, bad_frames;
#endif

void main_impl::do_update()
{

    float dt = timeline.previousFrameDuration();
    if (dt > 0)
    {
        if (fps_sample_timeline.currentFrameDuration() > 1.f/15)
        {
            fps_sample_timeline.nextFrame();
            constexpr float RC = 10;
            const float alpha = 1/(1 + RC);
            _frame_time = _frame_time*(1-alpha) + alpha*dt;
        }
        static size_t ctr = 0;
        if (dt >= 1.f/55 && !dt_expected.do_sleep)
            fm_debug("%zu frame took %.1f milliseconds", ctr++, dt*1e3f);
    }
    else
    {
        swapBuffers();
        timeline.nextFrame();
    }

    if (auto d = Math::abs(dt - dt_expected.value); d <= 4e-3f)
    {
        dt = dt_expected.value + 1e-6f;
#ifndef FM_NO_DEBUG
        ++good_frames;
#endif
    }
#ifndef FM_NO_DEBUG
    else if (dt_expected.has_focus && dt_expected.do_sleep && is_log_verbose()) [[unlikely]]
    {
        if (good_frames)
        {
            DBG_nospace << ++bad_frames << " bad frame " << d << ", expected:" << dt_expected.value << " good-frames:" << good_frames;
            good_frames = 0;
        }
    }
#endif

    dt = Math::clamp(dt, 1e-5f, Math::max(.2f, dt_expected.value));

    app.update(dt);
}

void main_impl::bind() noexcept
{
    framebuffer.fb.bind();
}

void main_impl::drawEvent()
{
    _shader.set_tint({1, 1, 1, 1});

    const auto clear_color = 0x222222ff_rgbaf;
#ifdef FM_USE_DEPTH32
    framebuffer.fb.clearColor(0, clear_color);
#else
    GL::defaultFramebuffer.clearColor(clear_color);
#endif
    draw_world();

    app.draw();
    GL::Renderer::flush();

    do_update();

#ifdef FM_USE_DEPTH32
    GL::Framebuffer::blit(framebuffer.fb, GL::defaultFramebuffer, framebuffer.fb.viewport(), GL::FramebufferBlit::Color);
#endif

    swapBuffers();
    redraw();
    timeline.nextFrame();

    if (dt_expected.do_sleep)
    {
        constexpr float ε = 1e-3f;
        const float Δt൦ = timeline.previousFrameDuration(), sleep_secs = dt_expected.value - Δt൦ - dt_expected.jitter;
        if (sleep_secs > ε)
            std::this_thread::sleep_for(std::chrono::nanoseconds((long long)(sleep_secs * 1e9f)));
        //fm_debug("jitter:%.1f sleep:%.0f", dt_expected.jitter*1000, sleep_secs*1000);
        const float Δt = timeline.previousFrameDuration() - dt_expected.value;
        constexpr float α = .1f;
        dt_expected.jitter = Math::max(dt_expected.jitter + Δt * α,
                                       dt_expected.jitter * (1-α) + Δt * α);
        dt_expected.jitter = std::copysign(std::fmin(dt_expected.value, std::fabs(dt_expected.jitter)), dt_expected.jitter);
    }
    else
        dt_expected.jitter = 0;
}

ArrayView<const clickable> main_impl::clickable_scenery() const noexcept
{
    return { _clickable_scenery.data(), _clickable_scenery.size() };
}

ArrayView<clickable> main_impl::clickable_scenery() noexcept
{
    return { _clickable_scenery.data(), _clickable_scenery.size() };
}

} // namespace floormat
