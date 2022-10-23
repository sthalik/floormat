#include "app.hpp"
#include "main/floormat-main.hpp"
#include "src/loader.hpp"

namespace floormat {

app::app() :
    _floor1{loader.tile_atlas("floor-tiles", {44, 4})},
    _floor2{loader.tile_atlas("metal1", {2, 2})},
    _wall1{loader.tile_atlas("wood2", {1, 1})},
    _wall2{loader.tile_atlas("wood1", {1, 1})},
    _fmain{floormat_main::create(*this, {})}
{
}

app::~app()
{
    loader_::destroy();
}

} // namespace floormat
