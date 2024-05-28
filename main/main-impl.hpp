#pragma once
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "src/world.hpp"
#include "src/timer.hpp"
#include "draw/ground.hpp"
#include "draw/wall.hpp"
#include "draw/anim.hpp"
#include "shaders/texture-unit-cache.hpp"
#include "shaders/shader.hpp"
#include "shaders/lightmap.hpp"
#include "main/clickable.hpp"
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Range.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/Platform/Sdl2Application.h>

#define FM_USE_DEPTH32

#ifdef FM_USE_DEPTH32
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/Texture.h>
#endif

namespace floormat {

struct floormat_app;
struct scenery;
class anim_atlas;
struct clickable;
class path_search;
class astar;

struct main_impl final : private Platform::Sdl2Application, public floormat_main
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

    struct lightmap_shader& lightmap_shader() noexcept override;

    class world& world() noexcept override;
    class world& reset_world() noexcept override;
    class world& reset_world(class world&& w) noexcept override;
    SDL_Window* window() noexcept override;
    void update_window_state();
    static unsigned get_window_refresh_rate(SDL_Window* window, unsigned min, unsigned max);
    float smoothed_frame_time() const noexcept override;

    fm_settings& settings() noexcept override;
    const fm_settings& settings() const noexcept override;

    global_coords pixel_to_tile(Vector2d position, int8_t z_level = 0) const noexcept override;
    Vector2d pixel_to_tile_(Vector2d position) const noexcept override;
    point pixel_to_point(Vector2d position, int8_t z_level = 0) const noexcept override;

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

    void cache_draw_on_startup();
    void clear_framebuffer();
    void drawEvent() override;
    void bind() noexcept override;
    void do_update(const Ns& dt);
    struct meshes meshes() noexcept override;

    bool is_text_input_active() const noexcept override;
    void start_text_input() noexcept override;
    void stop_text_input() noexcept override;

    void debug_callback(unsigned src, unsigned type, unsigned id, unsigned severity, StringView str) const;

    void set_cursor(uint32_t cursor) noexcept override;
    uint32_t cursor() const noexcept override;

    struct texture_unit_cache& texture_unit_cache() override;
    path_search& search() override;
    class astar& astar() override;

private:
    struct frame_timings_s
    {
        static constexpr unsigned min_refresh_rate = 20;

        unsigned refresh_rate;
        float smoothed_frame_time;
        bool vsync       : 1;
        bool minimized   : 1;
        bool focused     : 1;
    };

    frame_timings_s _frame_timings = {
        .refresh_rate = frame_timings_s::min_refresh_rate,
        .smoothed_frame_time = 0,
        .vsync = false,
        .minimized = false,
        .focused = true,
    };

    Time timeline;

    fm_settings s;
    [[maybe_unused]] char _dummy = (register_debug_callback(), '\0');
    floormat_app& app;
    struct texture_unit_cache _tuc;
    tile_shader _shader;
    struct lightmap_shader _lightmap_shader{_tuc};
    Array<clickable> _clickable_scenery;
    class world _world{};
    uint32_t _mouse_cursor = (uint32_t)-1;
    ground_mesh _ground_mesh;
    wall_mesh _wall_mesh;
    anim_mesh _anim_mesh;
#ifdef FM_USE_DEPTH32
    Framebuffer framebuffer;
#endif
    safe_ptr<path_search> _search;
    safe_ptr<class astar> _astar;

    void recalc_viewport(Vector2i fb_size, Vector2i win_size) noexcept;
    void draw_world() noexcept;

    template<typename Function> void draw_world_0(const Function& fun, const draw_bounds& d_b, const z_bounds& z_b, Vector2i win_size);

    draw_bounds get_draw_bounds() const noexcept override;

    void register_debug_callback();

    static Configuration make_conf(const fm_settings& s);
    static GLConfiguration make_gl_conf(const fm_settings& s);
    static Configuration::WindowFlags make_window_flags(const fm_settings& s);
};

} // namespace floormat
