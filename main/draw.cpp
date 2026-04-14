#include "main-impl.hpp"
#include "src/tile-constants.hpp"
#include "floormat/app.hpp"
#include "floormat/draw-bounds.hpp"
#include "src/camera-offset.hpp"
#include "src/anim-atlas.hpp"
#include "main/clickable.hpp"
#include "src/nanosecond.inl"
#include "src/fps-counter.hpp"
#include "shaders/shader.hpp"
#include "loader/loader.hpp"
#include "loader/sprite-atlas.hpp"
#include "src/chunk.hpp"
#include "compat/limits.hpp"
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>

namespace floormat {

namespace {

size_t bad_frame_counter = 0; // NOLINT
constexpr auto clear_color = 0x222222ff_rgbaf;

} // namespace

void main_impl::do_update(Ns dt)
{
    if (dt >= 500*Milliseconds) [[unlikely]]
    fm_debug("%zu frame took %.1f milliseconds",
             bad_frame_counter++, (double)(dt/Milliseconds));

    constexpr auto tenth_of_a_second = uint64_t(1e8);
    dt.stamp = Math::min(tenth_of_a_second, dt.stamp);

    app.update(dt);
    _frame_timings.fps_counter.update(dt);
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

template<std::invocable<chunk&, int16_t, int16_t, int8_t> Function>
void main_impl::draw_world_0(const Function& fun, ArrayView<chunk_coords_> chunks, Vector2i window_size, bool only, int8_t z_cur)
{
    if (chunks.isEmpty())
        return;

    Vector2s min{limits<int16_t>::max, limits<int16_t>::max},
             max{limits<int16_t>::min, limits<int16_t>::min};

    for (auto ch : chunks)
    {
        min = Math::min(min, {ch.x, ch.y});
        max = Math::max(max, {ch.x, ch.y});
    }

    fm_assert(max >= min);

    for (auto ch : chunks)
    {
        if (only && ch.z != z_cur)
            _shader.set_tint({1, 1, 1, 0.75});
        else
            _shader.set_tint({1, 1, 1, 1});

        auto* cʹ = _world.at(ch);
        if (!cʹ)
            continue;
        auto& c = *cʹ;
        bool is_visible;
        {
            const with_shifted_camera_offset o{_shader, ch};
            is_visible = check_chunk_visible(_shader.camera_offset(), window_size);
        }
        if (is_visible)
            fun(c, ch.x, ch.y, ch.z);
    }
}

void main_impl::draw_world() noexcept
{
    const auto z_bounds = app.get_z_bounds();
    const auto chunks  = get_draw_bounds(_chunk_bounds_array, {});
    const auto sz = window_size();

    arrayResize(_clickable_scenery, 0);
#ifdef FM_USE_DEPTH32
    framebuffer.fb.clearDepth(0);
#else
    GL::defaultFramebuffer.clearDepth(0);
#endif
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::setDepthMask(true);

    bind();

    _sprite_batch.clear();
    draw_world_0([&](chunk& c, int16_t, int16_t, int8_t) {
                    c.ensure_ground_mesh(_sprite_batch);
                    c.ensure_wall_mesh(_sprite_batch);
                 },
                 chunks, sz, z_bounds.only, z_bounds.cur);
    _sprite_batch.draw(_shader, false);  // depth-buffered opaque pass; no painter sort needed

    GL::Renderer::setDepthMask(false);
    _sprite_batch.clear();
    draw_world_0(
        [&](chunk& c, int16_t, int16_t, int8_t) {
            c.ensure_scenery_mesh(_sprite_batch, _do_render_vobjs);
            c.add_clickables(_shader, sz, _clickable_scenery, _do_render_vobjs);
        },
        chunks, sz, z_bounds.only, z_bounds.cur);

    _sprite_batch.draw(_shader);

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
