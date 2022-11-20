#pragma once
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "src/world.hpp"
#include "draw/floor.hpp"
#include "draw/wall.hpp"
#include "draw/anim.hpp"
#include "shaders/tile.hpp"

#include <Corrade/Containers/String.h>

#include <Magnum/Timeline.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/Platform/Sdl2Application.h>

namespace floormat {

struct floormat_app;

struct main_impl final : Platform::Sdl2Application, floormat_main
{
    explicit main_impl(floormat_app& app, fm_settings&& opts, int& fake_argc) noexcept;
    ~main_impl() noexcept override;

    int exec() override;
    void quit(int status) override;

    Magnum::Math::Vector2<int> window_size() const noexcept override;
    tile_shader& shader() noexcept override;
    const tile_shader& shader() const noexcept override;

    struct world& world() noexcept override;
    SDL_Window* window() noexcept override;

    fm_settings& settings() noexcept override;
    const fm_settings& settings() const noexcept override;

    global_coords pixel_to_tile(Vector2d position) const noexcept override;

    [[maybe_unused]] void viewportEvent(ViewportEvent& event) override;
    [[maybe_unused]] void mousePressEvent(MouseEvent& event) override;
    [[maybe_unused]] void mouseReleaseEvent(MouseEvent& event) override;
    [[maybe_unused]] void mouseMoveEvent(MouseMoveEvent& event) override;
    [[maybe_unused]] void mouseScrollEvent(MouseScrollEvent& event) override;
    [[maybe_unused]] void textInputEvent(TextInputEvent& event) override;
    //[[maybe_unused]] void textEditingEvent(TextEditingEvent& event) override;
    [[maybe_unused]] void keyPressEvent(KeyEvent& event) override;
    [[maybe_unused]] void keyReleaseEvent(KeyEvent& event) override;
    [[maybe_unused]] void anyEvent(SDL_Event& event) override;
    
    void drawEvent() override;
    void do_update();
    void update_window_state();

    bool is_text_input_active() const noexcept override;
    void start_text_input() noexcept override;
    void stop_text_input() noexcept override;

    void debug_callback(unsigned src, unsigned type, unsigned id, unsigned severity, const std::string& str) const;

private:
    fm_settings s;
    [[maybe_unused]] char _dummy = maybe_register_debug_callback(s.gpu_debug);
    floormat_app& app; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    tile_shader _shader;
    struct world _world{};
    floor_mesh _floor_mesh;
    wall_mesh _wall_mesh;
    anim_mesh _anim_mesh;
    Magnum::Timeline timeline;
    struct {
        float value = 0;
        float jitter = 0;
        bool do_sleep = false;
    } dt_expected;

    struct draw_bounds final { std::int16_t minx, maxx, miny, maxy; };

    GL::Framebuffer _msaa_framebuffer{NoCreate};
    GL::Renderbuffer _msaa_depth{NoCreate};
    GL::Renderbuffer _msaa_color{NoCreate};

    void recalc_viewport(Vector2i size) noexcept;
    void draw_world() noexcept;
    void draw_anim() noexcept;

    draw_bounds get_draw_bounds() const noexcept;
    [[nodiscard]] static bool check_chunk_visible(const Vector2d& offset, const Vector2i& size) noexcept;

    char maybe_register_debug_callback(fm_gpu_debug flag);
    void register_debug_callback();

    static Configuration make_conf(const fm_settings& s);
    static GLConfiguration make_gl_conf(const fm_settings& s);
    static Configuration::WindowFlags make_window_flags(const fm_settings& s);
};

} // namespace floormat
