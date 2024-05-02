#include "main-impl.hpp"
#include "src/tile-constants.hpp"
#include "floormat/app.hpp"
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

    do_update(timeline.update());

#ifdef FM_USE_DEPTH32
    GL::Framebuffer::blit(framebuffer.fb, GL::defaultFramebuffer, framebuffer.fb.viewport(), GL::FramebufferBlit::Color);
#endif

    swapBuffers();
    redraw();
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
                auto* cʹ = _world.at(ch);
                if (!cʹ)
                    continue;
                auto& c = *cʹ;
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
                auto* cʹ = _world.at(pos);
                if (!cʹ)
                    continue;
                auto& c = *cʹ;
                const with_shifted_camera_offset o{_shader, pos, {minx, miny}, {maxx, maxy}};
                if (check_chunk_visible(_shader.camera_offset(), sz))
                    _anim_mesh.draw(_shader, sz, c, _clickable_scenery, _do_render_vobjs);
            }

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
