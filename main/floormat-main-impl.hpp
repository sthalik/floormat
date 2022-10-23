#pragma once
#include "floormat.hpp"
#include "floormat-main.hpp"
#include "shaders/tile-shader.hpp"
#include <Corrade/Containers/String.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/Platform/Sdl2Application.h>

#define FM_MSAA

namespace floormat {

struct floormat_app;

struct main_impl final : Platform::Sdl2Application, floormat_main
{
    main_impl(floormat_app& app, const fm_options& opts);
    ~main_impl() override;

    void quit(int status) override;

    Magnum::Math::Vector2<int> window_size() const noexcept override;
    tile_shader& shader() noexcept override;
    void register_debug_callback() noexcept override;

    struct world& world() noexcept override;
    SDL_Window* window() noexcept override;
    float smoothed_dt() const noexcept override;

    [[maybe_unused]] void viewportEvent(ViewportEvent& event) override;
    [[maybe_unused]] void mousePressEvent(MouseEvent& event) override;
    [[maybe_unused]] void mouseReleaseEvent(MouseEvent& event) override;
    [[maybe_unused]] void mouseMoveEvent(MouseMoveEvent& event) override;
    [[maybe_unused]] void mouseScrollEvent(MouseScrollEvent& event) override;
    [[maybe_unused]] void textInputEvent(TextInputEvent& event) override;
    [[maybe_unused]] void keyPressEvent(KeyEvent& event) override;
    [[maybe_unused]] void keyReleaseEvent(KeyEvent& event) override;
    [[maybe_unused]] void anyEvent(SDL_Event& event) override;
    void drawEvent() override;

private:
    float _frame_time = 0;
    floormat_app& app;
    fm_options s;
    tile_shader _shader;
    Magnum::Timeline timeline;
    int fake_argc = 1;

#ifdef FM_MSAA
    GL::Framebuffer _msaa_framebuffer{{{}, windowSize()}};
    GL::Renderbuffer _msaa_renderbuffer{};
#endif

    void recalc_viewport(Vector2i size);

    void debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                        GL::DebugOutput::Severity severity, const std::string& str) const;
    static void _debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                                GL::DebugOutput::Severity severity, const std::string& str, const void* self);

    static Configuration make_conf(const fm_options& s);
    static GLConfiguration make_gl_conf(const fm_options& s);
    static Configuration::WindowFlags make_window_flags(const fm_options& s);
};

} // namespace floormat
