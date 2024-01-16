#pragma once

#include "compat/defs.hpp"
#include "src/global-coords.hpp"
#include <Magnum/Math/Vector2.h>

struct SDL_Window;
namespace Magnum::Platform { class Sdl2Application; }

namespace floormat {

template<typename T> class safe_ptr;
struct fm_settings;
struct floormat_app;
struct tile_shader;
struct lightmap_shader;
class world;
struct scenery;
class anim_atlas;
struct clickable;
struct ground_mesh;
struct wall_mesh;
struct anim_mesh;
struct texture_unit_cache;
class path_search;
class astar;

struct floormat_main
{
    struct draw_bounds final { int16_t minx, maxx, miny, maxy; };
    struct meshes final {
        ground_mesh& ground;
        wall_mesh& wall;
        anim_mesh& anim;
    };

    floormat_main() noexcept;
    virtual ~floormat_main() noexcept;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(floormat_main);
    fm_DECLARE_DEPRECATED_MOVE_ASSIGNMENT(floormat_main);

    virtual Platform::Sdl2Application& application() noexcept = 0;
    virtual const Platform::Sdl2Application& application() const noexcept = 0;

    virtual int exec() = 0;
    virtual void quit(int status) = 0;

    virtual Magnum::Math::Vector2<int> window_size() const noexcept;
    virtual tile_shader& shader() noexcept = 0;
    virtual struct lightmap_shader& lightmap_shader() noexcept = 0;
    virtual const tile_shader& shader() const noexcept = 0;
    virtual void bind() noexcept = 0;
    constexpr float smoothed_dt() const noexcept { return _frame_time; }
    virtual fm_settings& settings() noexcept = 0;
    virtual const fm_settings& settings() const noexcept = 0;

    virtual bool is_text_input_active() const noexcept = 0;
    virtual void start_text_input() noexcept = 0;
    virtual void stop_text_input() noexcept = 0;

    virtual ArrayView<const clickable> clickable_scenery() const noexcept = 0;
    virtual ArrayView<clickable> clickable_scenery() noexcept = 0;
    virtual void set_cursor(uint32_t cursor) noexcept = 0;
    virtual uint32_t cursor() const noexcept = 0;

    virtual global_coords pixel_to_tile(Vector2d position) const noexcept = 0;
    virtual Vector2d pixel_to_tile_(Vector2d position) const noexcept = 0;
    virtual draw_bounds get_draw_bounds() const noexcept = 0;
    [[nodiscard]] static bool check_chunk_visible(const Vector2d& offset, const Vector2i& size) noexcept;
    virtual struct meshes meshes() noexcept = 0;

    virtual class world& world() noexcept = 0;
    virtual class world& reset_world() noexcept = 0;
    virtual class world& reset_world(class world&& w) noexcept = 0;
    virtual SDL_Window* window() noexcept = 0;
    Vector2 dpi_scale() const noexcept { return _dpi_scale; }
    static int get_mods() noexcept;

    void set_render_vobjs(bool value);
    bool is_rendering_vobjs() const;

    virtual struct texture_unit_cache& texture_unit_cache() = 0;
    virtual path_search& search() = 0;
    virtual class astar& astar() = 0;

    [[nodiscard]] static floormat_main* create(floormat_app& app, fm_settings&& options);
    [[maybe_unused]] static void debug_break();

protected:
    float _frame_time = 0;
    Vector2 _dpi_scale{1, 1}, _virtual_scale{1, 1};
    Vector2i _framebuffer_size;
    bool _do_render_vobjs : 1 = true;
};

} // namespace floormat
