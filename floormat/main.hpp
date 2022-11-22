#pragma once

#include "compat/defs.hpp"
#include "src/global-coords.hpp"
#include <Magnum/Math/Vector2.h>

struct SDL_Window;
namespace Corrade::Containers { template<typename T> class ArrayView; }

namespace floormat {

struct fm_settings;
struct floormat_app;
struct tile_shader;
struct world;
struct scenery;
struct anim_atlas;
template<typename Atlas, typename T> struct clickable;

struct floormat_main
{
    floormat_main() noexcept;
    virtual ~floormat_main() noexcept;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(floormat_main);
    fm_DECLARE_DEPRECATED_MOVE_ASSIGNMENT(floormat_main);

    virtual int exec() = 0;
    virtual void quit(int status) = 0;

    virtual Magnum::Math::Vector2<int> window_size() const noexcept = 0;
    virtual tile_shader& shader() noexcept = 0;
    virtual const tile_shader& shader() const noexcept = 0;
    constexpr float smoothed_dt() const noexcept { return _frame_time1; }
    virtual fm_settings& settings() noexcept = 0;
    virtual const fm_settings& settings() const noexcept = 0;

    virtual bool is_text_input_active() const noexcept = 0;
    virtual void start_text_input() noexcept = 0;
    virtual void stop_text_input() noexcept = 0;

    virtual ArrayView<const clickable<anim_atlas, scenery>> clickable_scenery() const noexcept = 0;
    virtual ArrayView<clickable<anim_atlas, scenery>> clickable_scenery() noexcept = 0;
    virtual void set_cursor(std::uint32_t cursor) noexcept = 0;
    virtual std::uint32_t cursor() const noexcept = 0;

    virtual global_coords pixel_to_tile(Vector2d position) const noexcept = 0;

    virtual struct world& world() noexcept = 0;
    virtual SDL_Window* window() noexcept = 0;
    float dpi_scale() const noexcept { return _dpi_scale; }

    [[nodiscard]] static floormat_main* create(floormat_app& app, fm_settings&& options);

protected:
    float _frame_time1 = 0, _frame_time2 = 0, _dpi_scale = 1;
};

} // namespace floormat
