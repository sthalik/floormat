#pragma once
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "src/world.hpp"
#include "draw/floor.hpp"
#include "draw/wall.hpp"
#include "draw/anim.hpp"
#include "shaders/tile.hpp"
#include "main/clickable.hpp"
#include <vector>
#include <Corrade/Containers/String.h>
#include <Magnum/Timeline.h>
#include <Magnum/Math/Range.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/Platform/Sdl2Application.h>

//#define FM_USE_DEPTH32

#ifdef FM_USE_DEPTH32
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/Texture.h>
#endif

namespace floormat {

struct floormat_app;
struct scenery;
struct anim_atlas;
struct clickable;

struct main_impl final : Platform::Sdl2Application, floormat_main
{
#ifdef FM_USE_DEPTH32
    struct Framebuffer final {
        GL::Framebuffer fb{NoCreate};
        GL::Renderbuffer depth{NoCreate};
        GL::Texture2D color{NoCreate};
    };
#endif

    explicit main_impl(floormat_app& app, fm_settings&& opts, int& argc, char** argv) noexcept;
    ~main_impl() noexcept override;

    int exec() override;
    void quit(int status) override;

    tile_shader& shader() noexcept override;
    const tile_shader& shader() const noexcept override;

    struct world& world() noexcept override;
    struct world& reset_world() noexcept override;
    struct world& reset_world(struct world&& w) noexcept override;
    SDL_Window* window() noexcept override;

    fm_settings& settings() noexcept override;
    const fm_settings& settings() const noexcept override;

    global_coords pixel_to_tile(Vector2d position) const noexcept override;
    Vector2d pixel_to_tile_(Vector2d position) const noexcept override;

    ArrayView<const clickable> clickable_scenery() const noexcept override;
    ArrayView<clickable> clickable_scenery() noexcept override;

    Platform::Sdl2Application& application() noexcept override;
    const Platform::Sdl2Application& application() const noexcept override;

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
    struct meshes meshes() noexcept override;

    bool is_text_input_active() const noexcept override;
    void start_text_input() noexcept override;
    void stop_text_input() noexcept override;

    void debug_callback(unsigned src, unsigned type, unsigned id, unsigned severity, StringView str) const;

    void set_cursor(uint32_t cursor) noexcept override;
    uint32_t cursor() const noexcept override;

private:
    fm_settings s;
    [[maybe_unused]] char _dummy = (register_debug_callback(), '\0');
    floormat_app& app; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    tile_shader _shader;
    std::vector<clickable> _clickable_scenery;
    struct world _world{};
    Magnum::Timeline timeline;
    uint32_t _mouse_cursor = (uint32_t)-1;
    floor_mesh _floor_mesh;
    wall_mesh _wall_mesh;
    anim_mesh _anim_mesh;
#ifdef FM_USE_DEPTH32
    Framebuffer framebuffer;
#endif

    struct {
        float value = 0;
        float jitter = 0;
        bool do_sleep = false;
    } dt_expected;

    Timeline fps_sample_timeline;

    void recalc_viewport(Vector2i fb_size, Vector2i win_size) noexcept;
    void draw_world() noexcept;

    draw_bounds get_draw_bounds() const noexcept override;

    void register_debug_callback();

    static Configuration make_conf(const fm_settings& s);
    static GLConfiguration make_gl_conf(const fm_settings& s);
    static Configuration::WindowFlags make_window_flags(const fm_settings& s);
};

} // namespace floormat
