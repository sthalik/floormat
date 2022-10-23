#pragma once

#include "floormat.hpp"
#include "src/global-coords.hpp"
#include <Magnum/Math/Vector2.h>

struct SDL_Window;

namespace floormat {

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

    virtual global_coords pixel_to_tile(Vector2d position) const noexcept = 0;

    virtual world& world() noexcept = 0;
    virtual SDL_Window* window() noexcept = 0;

    static floormat_main* create(floormat_app& app, const fm_options& options);

protected:
    float _frame_time = 0;
};

} // namespace floormat
