#include "main-impl.hpp"
#include "floormat/app.hpp"
#include <cr/StringView.h>
#include <cr/StringIterable.h>
#include <mg/Functions.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <algorithm> // todo std::minmax

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
    timeline = Time::now();
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
#ifdef FM_USE_DEPTH32
        .setDepthBufferSize(0)
#endif
        .setColorBufferSize({8, 8, 8, 0})
        .setStencilBufferSize(0);
}

unsigned get_window_refresh_rate(SDL_Window* window, unsigned min, unsigned max)
{
    fm_assert(window != nullptr);
    if (int index = SDL_GetWindowDisplayIndex(window); index < 0)
        fm_warn_once("SDL_GetWindowDisplayIndex: %s", SDL_GetError());
    else if (SDL_DisplayMode dpymode{}; SDL_GetCurrentDisplayMode(index, &dpymode) < 0)
        fm_warn_once("SDL_GetCurrentDisplayMode: %s", SDL_GetError());
    else
    {
        fm_assert(dpymode.refresh_rate > 0 && dpymode.refresh_rate < max);
        return (unsigned)dpymode.refresh_rate;
    }
    return min;
}

void main_impl::update_window_state() // todo window minimized, out of focus, fake vsync etc
{
    auto refresh_rate = get_window_refresh_rate(window(), _frame_timings.min_refresh_rate, 10000);
    fm_assert(refresh_rate > 0 && refresh_rate < 1000);
    _frame_timings = {
        .refresh_rate = refresh_rate,
    };
}

auto main_impl::meshes() noexcept -> struct meshes
{
    return { _ground_mesh, _wall_mesh, _anim_mesh, };
};

class world& main_impl::reset_world() noexcept
{
    return reset_world(floormat::world{});
}

} // namespace floormat
