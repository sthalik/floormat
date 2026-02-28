#include "main-impl.hpp"
#include "src/tile-constants.hpp"
#include "floormat/app.hpp"
#include "floormat/draw-bounds.hpp"
#include "src/camera-offset.hpp"
#include "src/anim-atlas.hpp"
#include "main/clickable.hpp"
#include "src/nanosecond.inl"
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>

namespace floormat {

namespace {

size_t bad_frame_counter = 0; // NOLINT
constexpr auto clear_color = 0x222222ff_rgbaf;

} // namespace

void main_impl::do_update(const Ns& dtʹ)
{
    constexpr auto eps = 1e-5f;
    constexpr auto max_dt = Milliseconds*100;
    const auto dt = dtʹ > max_dt ? max_dt : dtʹ;
    if (float secs{Time::to_seconds(dt)}; secs > eps)
    {
#if 1
        constexpr float RC = 60;
        constexpr float alpha = 1 / (1 + RC);
        float& value = _frame_timings.smoothed_frame_time;
        value = (value*(1 - alpha) + alpha * secs);
#else
        value = secs;
#endif
        if (secs > 35e-3f /* && !dt_expected.do_sleep */) [[likely]]
            fm_debug("%zu frame took %.2f milliseconds", bad_frame_counter++, (double)secs*1e3);
    }
    else
        swapBuffers();

    app.update(dt);
}

void main_impl::clear_framebuffer()
{
#ifdef FM_USE_DEPTH32
    framebuffer.fb.clearColor(0, clear_color);
#else
    GL::defaultFramebuffer.clearColor(clear_color);
#endif
}

void main_impl::cache_draw_on_startup()
{
    _shader.set_tint({1, 1, 1, 1});
    clear_framebuffer();
    for (int i = 0; i < 3; i++)
    {
        do_update(Ns{1});
        draw_world();
    }
    clear_framebuffer();
    (void)timeline.update();
    swapBuffers();
    redraw();
}

void main_impl::drawEvent()
{
    if (_first_frame) [[unlikely]]
    {
        _first_frame = false;
        cache_draw_on_startup();
    }

    _shader.set_tint({1, 1, 1, 1});

    clear_framebuffer();
    draw_world();

    app.draw();
    GL::Renderer::flush();

    auto dt = timeline.update();
#if 1
    do_update(dt);
#else
    do_update(Second/60 + Ns{1});
#endif

#ifdef FM_USE_DEPTH32
    GL::Framebuffer::blit(framebuffer.fb, GL::defaultFramebuffer, framebuffer.fb.viewport(), GL::FramebufferBlit::Color);
#endif

    swapBuffers();
    redraw();
}

template<typename Function>
void main_impl::draw_world_0(const Function& fun, const draw_bounds& draw_bounds, const z_bounds& z_bounds, Vector2i window_size)
{
    const auto [z_min, z_max, z_cur, only] = z_bounds;
    const auto [minx, maxx, miny, maxy] = draw_bounds;

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
                auto* cʹ = _world.at(ch);
                if (!cʹ)
                    continue;
                auto& c = *cʹ;

                const with_shifted_camera_offset o{_shader, ch, {minx, miny}, {maxx, maxy}};
                if (check_chunk_visible(_shader.camera_offset(), window_size))
                    fun(c, x, y, z);
            }
    }
}


void main_impl::draw_world() noexcept
{
    const auto z_bounds = app.get_z_bounds();
    const auto draw_bounds = get_draw_bounds();
    const auto sz = window_size();

    fm_assert(1 + draw_bounds.maxx - draw_bounds.minx <= 8);
    fm_assert(1 + draw_bounds.maxy - draw_bounds.miny <= 8);

    arrayResize(_clickable_scenery, 0);
#ifdef FM_USE_DEPTH32
        framebuffer.fb.clearDepth(0);
#else
        GL::defaultFramebuffer.clearDepth(0);
#endif
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::setDepthMask(true);

    bind();

    draw_world_0([&](chunk& c, int16_t, int16_t, int8_t) {
                     _ground_mesh.draw(_shader, c);
                     _wall_mesh.draw(_shader, c);
                 },
                 draw_bounds, z_bounds, sz);

    GL::Renderer::setDepthMask(false);

    draw_world_0([&](chunk& c, int16_t, int16_t, int8_t) {
                     _anim_mesh.draw(_shader, sz, c, _clickable_scenery, _do_render_vobjs);
                 },
                 draw_bounds, z_bounds, sz);

    _shader.set_tint({1, 1, 1, 1});
    GL::Renderer::setDepthMask(true);

    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
}

void main_impl::bind() noexcept
{
    framebuffer.fb.bind();
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
