#include "main-impl.hpp"
#include "clickable.hpp"
#include "floormat/app.hpp"
#include "floormat/draw-bounds.hpp"
#include "src/anim-atlas.hpp"
#include "src/fps-counter.hpp"
#include "src/chunk.hpp"
#include "src/object.hpp"
#include "src/object-storage.inl"
#include "shaders/shader.hpp"
#include "compat/function-view.hpp"
#include "compat/fwd/algorithm/sort.hpp"
#include "compat/nanosecond.inl"
#include <cr/GrowableArray.h>
#include <cr/ArrayView.h>
#include <mg/DefaultFramebuffer.h>
#include <mg/Renderer.h>
#include <mg/Color.h>

namespace floormat {

namespace {

usize bad_frame_counter = 0; // NOLINT todo thread safety
constexpr auto clear_color = 0x222222ff_rgbaf;
constexpr auto cornflower_blue = 0x6495edff_rgbaf;

} // namespace

void main_impl::do_update(Ns dt)
{
    _frame_timings.fps_counter.update(dt);

    if (const auto hz = s.fixed_framerate) [[unlikely]]
    {
        const auto step = Second/hz;
        dt = step == Ns{} ? Ns{1} : step;
    }

    if (dt >= 500*Milliseconds) [[unlikely]]
    fm_debug("%zu frame took %.1f milliseconds",
             bad_frame_counter++, (double)(dt/Milliseconds));

    dt = Math::min(dt, 100*Milliseconds);

    app.update(dt);
}

void main_impl::clear_framebuffer()
{
    framebuffer.fb.clearColor(0, clear_color);
}

void main_impl::cache_draw_on_startup()
{
    _shader.set_tint({1, 1, 1, 1});
    clear_framebuffer();
#ifndef FLOORMAT_NO_PGO_DRIVER
    if (!s.no_warmup)
#endif
        for (int i = 0; i < 3; i++)
        {
            // Not do_update(): these synthetic ticks aren't real frame times and
            // would otherwise seed the FPS counter's settle-phase average with 1e9.
            app.update(Ns{1});
            draw_world();
        }
    clear_framebuffer();
    (void)timeline.update();
    swapBuffers();
    redraw();
}

void main_impl::drawEvent()
{
    const auto t0 = Time::now();

    float ddpi = 96, hdpi = 96, vdpi = 96;

    if (auto dpi_override = commandLineDpiScaling())
        _dpi_scale = *dpi_override;
    else if (!SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window()), &ddpi, &hdpi, &vdpi))
        _dpi_scale = Vector2{hdpi, vdpi} / 96;
    else
        _dpi_scale = Vector2{1};

    if (_first_frame) [[unlikely]]
    {
        _first_frame = false;
        cache_draw_on_startup();
        if (s.driver_no_swapbuffers) [[unlikely]]
        {
            // Not clear_framebuffer(): that one clears the offscreen buffer, which reaches the
            // screen only through the blit below. This is the last swapBuffers() of the run.
            GL::defaultFramebuffer.clearColor(cornflower_blue);
            swapBuffers();
        }
    }

    _shader.set_tint({1, 1, 1, 1});

    clear_framebuffer();
    draw_world();

    app.draw();
    GL::Renderer::flush();

    do_update(timeline.update());

    if (!s.driver_no_swapbuffers) [[likely]]
    {
        GL::Framebuffer::blit(framebuffer.fb, GL::defaultFramebuffer, framebuffer.fb.viewport(), GL::FramebufferBlit::Color);
        swapBuffers();
    }
    redraw();

    if (_frame_times_file) [[unlikely]]
        std::fprintf(_frame_times_file, "%llu\n", (unsigned long long)(Time::now() - t0).stamp);

    if (s.show_fps) [[unlikely]]
    {
        const auto now = Time::now();
        if (now - _fps_log_time >= 5*Seconds)
        {
            if (float fps = smoothed_fps(); fps > 0)
            {
                std::fprintf(stderr, "FPS: %.1f\n", (double)fps);
                _fps_log_time = now;
            }
        }
    }
}

template<std::invocable<chunk&, i16, i16, i8> Function>
void main_impl::draw_world_0(const Function& fun, ArrayView<chunk_coords_> chunks)
{
    for (auto ch : chunks)
    {
        auto* cʹ = _world.at(ch);
        if (!cʹ)
            continue;
        fun(*cʹ, ch.x, ch.y, ch.z);
    }
}

void main_impl::draw_world() noexcept
{
    const auto z_bounds = app.get_z_bounds();
    const auto chunks  = get_draw_bounds(_chunk_bounds_array, {});
    const auto sz = window_size();
    _drawn_camera_offset = _shader.camera_offsetʹ();
    _drawn_window_size = sz;

    // Draw chunks front to back for early depth rejection.
    // Keep this sort out of get_draw_bounds(), which also determines object update order.
    std::sort(chunks.begin(), chunks.end(), [](chunk_coords_ a, chunk_coords_ b) {
        constexpr auto w = chunk_size<i32>;
        return w * ((i32)a.x + a.y) + tile_size_z * a.z
             > w * ((i32)b.x + b.y) + tile_size_z * b.z;
    });

    arrayResize(_clickable_scenery, 0);
    framebuffer.fb.clearDepth(0);
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::setDepthMask(true);

    bind();

    // The tint uniform uploads once per SpriteBatch::draw, so dimming other
    // z-levels needs its own batch, drawn underneath the current level's.
    const auto draw_batches = [&](const auto& emit, bool do_sort)
    {
        if (z_bounds.only)
        {
            _sprite_batch.clear();
            draw_world_0([&](chunk& c, i16 x, i16 y, i8 z) {
                if (z != z_bounds.cur)
                    emit(c, x, y, z);
            }, chunks);
            _shader.set_tint({1, 1, 1, 0.75});
            _sprite_batch.draw(_shader, do_sort);
        }
        _sprite_batch.clear();
        draw_world_0([&](chunk& c, i16 x, i16 y, i8 z) {
            if (!z_bounds.only || z == z_bounds.cur)
                emit(c, x, y, z);
        }, chunks);
        _shader.set_tint({1, 1, 1, 1});
        _sprite_batch.draw(_shader, do_sort);
    };

    // Walls first, so early-Z rejects the ground behind them. A translucent wall texel
    // would hide the ground, since blending stays on and this pass writes depth.
    draw_batches([&](chunk& c, i16, i16, i8) {
        c.ensure_wall_mesh(_sprite_batch);
        c.ensure_ground_mesh(_sprite_batch);
    }, false);  // depth-buffered opaque pass; no painter sort needed

    GL::Renderer::setDepthMask(false);

    draw_batches([&](chunk& c, i16, i16, i8) {
        c.ensure_scenery_mesh(_sprite_batch, _do_render_vobjs);
    }, true);

    GL::Renderer::setDepthMask(true);

    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    if (_do_collect_clickables)
        draw_world_0([&](chunk& c, i16, i16, i8) {
            for (const auto& eʹ : c.objects())
                if (_do_render_vobjs || !eʹ->is_virtual())
                    if (auto cl = clickable_for_object(*eʹ, sz, _drawn_camera_offset))
                        arrayAppend(_clickable_scenery, *cl);
        }, chunks);
}

void main_impl::pick_clickable(Vector2i pixel, function_view<void(const clickable&) const> on_pick)
{
    Optional<clickable> best;
    u32 depth = 0;

    draw_world_0([&](chunk& c, i16, i16, i8) {
        for (const auto& eʹ : c.objects())
        {
            const object& e = *eʹ;
            if (!_do_render_vobjs && e.is_virtual())
                continue;
            auto cl = clickable_for_object(e, _drawn_window_size, _drawn_camera_offset);
            if (cl && cl->depth > depth && clickable_hit_test(*cl, pixel))
            {
                depth = cl->depth;
                best = cl;
            }
        }
    }, _chunk_bounds_array);

    if (best)
        on_pick(*best);
}

ArrayView<const chunk_coords_> main_impl::drawn_chunks() const noexcept
{
    return { _chunk_bounds_array.data(), _chunk_bounds_array.size() };
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
