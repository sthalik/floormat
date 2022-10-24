#pragma once

#include "src/global-coords.hpp"
#include <Magnum/Math/Vector2.h>

struct SDL_Window;

namespace floormat {

struct fm_settings;
struct floormat_app;
struct tile_shader;
struct world;

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
    virtual void* register_debug_callback() noexcept = 0;
    constexpr float smoothed_dt() const noexcept { return _frame_time; }
    virtual fm_settings& settings() noexcept = 0;
    virtual const fm_settings& settings() const noexcept = 0;

    virtual bool is_text_input_active() const noexcept = 0;
    virtual void start_text_input() noexcept = 0;
    virtual void stop_text_input() noexcept = 0;

    virtual global_coords pixel_to_tile(Vector2d position) const noexcept = 0;

    virtual world& world() noexcept = 0;
    virtual SDL_Window* window() noexcept = 0;

    static floormat_main* create(floormat_app& app, fm_settings&& options);

protected:
    float _frame_time = 0;
};

} // namespace floormat
