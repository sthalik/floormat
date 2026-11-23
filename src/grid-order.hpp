#pragma once
#include "global-coords.hpp"
#include "grid-pass.hpp"
#include <type_traits>

namespace floormat {

// One grid that a read-only query found unbuilt, for the main thread to build, or one cover grid octant to fill.
struct grid_order
{
    enum class grid_kind : u8 { pass, object, cover };

    Grid::Pass::Params pass_params; // pass and object: the pool's params
    u32 cover_div_size = 0;         // cover: Grid::Cover::Params::div_size
    chunk_coords_ chunk;
    grid_kind kind;
    u8 octant = 0;                  // cover: 0 to 31
};

// chunk_coords_'s copy assignment is user-provided, so grid_order isn't trivially copyable.
static_assert(std::is_trivially_copy_constructible_v<grid_order> && std::is_trivially_destructible_v<grid_order>);

} // namespace floormat
