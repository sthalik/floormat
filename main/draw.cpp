#include "main-impl.hpp"
#include "floormat/app.hpp"
#include "src/camera-offset.hpp"
#include "src/anim-atlas.hpp"
#include "main/clickable.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>
#include <algorithm>
#include <thread>

namespace floormat {

void main_impl::recalc_viewport(Vector2i size) noexcept
{
    update_window_state();
    _shader.set_scale(Vector2{size});
    GL::defaultFramebuffer.setViewport({{}, size });

    // -- state ---
    using R = GL::Renderer;
    GL::Renderer::setBlendEquation(R::BlendEquation::Add, R::BlendEquation::Add);
    GL::Renderer::setBlendFunction(R::BlendFunction::SourceAlpha, R::BlendFunction::OneMinusSourceAlpha);
    GL::Renderer::disable(R::Feature::FaceCulling);
    GL::Renderer::disable(R::Feature::DepthTest);
    GL::Renderer::enable(R::Feature::Blending);
    GL::Renderer::enable(R::Feature::ScissorTest);
    GL::Renderer::enable(R::Feature::DepthClamp);
    GL::Renderer::setDepthFunction(R::DepthFunction::Greater);
    GL::Renderer::setScissor({{}, size});

    // -- user--
    app.on_viewport_event(size);
}

global_coords main_impl::pixel_to_tile(Vector2d position) const noexcept
{
    constexpr Vector2d pixel_size(TILE_SIZE2);
    constexpr Vector2d half{.5, .5};
    const Vector2d px = position - Vector2d{windowSize()}*.5 - _shader.camera_offset();
    const Vector2d vec = tile_shader::unproject(px*.5) / pixel_size + half;
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
    const auto [minx, maxx, miny, maxy] = get_draw_bounds();
    const auto sz = windowSize();

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            const chunk_coords pos{x, y};
            if (!_world.contains(pos))
                app.maybe_initialize_chunk(pos, _world[pos]);
            auto& c = _world[pos];
            if (c.empty())
                continue;
            const with_shifted_camera_offset o{_shader, pos, {minx, miny}, {maxx, maxy}};
            if (check_chunk_visible(_shader.camera_offset(), sz))
                _floor_mesh.draw(_shader, c);
        }

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::defaultFramebuffer.clearDepthStencil(0, 0);
    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            const chunk_coords pos{x, y};
            auto& c = _world[pos];
            if (c.empty())
                continue;
            const with_shifted_camera_offset o{_shader, pos, {minx, miny}, {maxx, maxy}};
            if (check_chunk_visible(_shader.camera_offset(), sz))
                _wall_mesh.draw(_shader, c);
        }
}

void main_impl::draw_anim() noexcept
{
    const auto sz = windowSize();
    const auto [minx, maxx, miny, maxy] = get_draw_bounds();
    _clickable_scenery.clear();

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            const chunk_coords pos{x, y};
            auto& c = _world[pos];
            if (c.empty())
                continue;
            const with_shifted_camera_offset o{_shader, pos, {minx, miny}, {maxx, maxy}};
            if (check_chunk_visible(_shader.camera_offset(), sz))
                for (std::size_t i = 0; i < TILE_COUNT; i++)
                {
                    const local_coords xy{i};
                    if (auto [atlas, s] = c[xy].scenery(); atlas)
                    {
                        _anim_mesh.draw(_shader, *atlas, s.r, s.frame, xy);
                        const auto& g = atlas->group(s.r);
                        const auto& f = atlas->frame(s.r, s.frame);
                        const auto world_pos = TILE_SIZE20 * Vector3(xy.x, xy.y, 0) + Vector3(g.offset);
                        const Vector2ui offset((Vector2(_shader.camera_offset()) + Vector2(sz)*.5f)
                                               + _shader.project(world_pos) - Vector2(f.ground));
                        clickable<anim_atlas, scenery> item = {
                            *atlas, s,
                            { f.offset, f.offset + f.size }, { offset, offset + f.size },
                            atlas->bitmask(), tile_shader::depth_value(xy, 0.25f), pos, xy,
                            !g.mirror_from.isEmpty(),
                        };
                        _clickable_scenery.push_back(item);
                    }
                }
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

void main_impl::do_update()
{
    float dt = timeline.previousFrameDuration();
    if (dt > 0)
    {
        const float RC1 = dt_expected.do_sleep ? 1.f : 1.f/5, RC2 = 1.f/10;
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

    dt = std::clamp(dt, 1e-5f, std::fmaxf(5e-2f, dt_expected.value));

    app.update(dt);
}

void main_impl::drawEvent()
{
    _dpi_scale = 1;
    if (int index = SDL_GetWindowDisplayIndex(window()); index >= 0)
        if (float dpi = 96; !SDL_GetDisplayDPI(index, &dpi, nullptr, nullptr))
            _dpi_scale = dpi / 96;

    _shader.set_tint({1, 1, 1, 1});

    {
        _shader.set_tint({1, 1, 1, 1});
        const auto clear_color = 0x222222ff_rgbaf;
        GL::defaultFramebuffer.clearColor(clear_color);
        draw_world();
        draw_anim();
        GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
    }

    app.draw();
    GL::Renderer::flush();

    do_update();

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

ArrayView<const clickable<anim_atlas, scenery>> main_impl::clickable_scenery() const noexcept
{
    return { _clickable_scenery.data(), _clickable_scenery.size() };
}

ArrayView<clickable<anim_atlas, scenery>> main_impl::clickable_scenery() noexcept
{
    return { _clickable_scenery.data(), _clickable_scenery.size() };
}

} // namespace floormat
