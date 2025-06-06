#include "app.hpp"
#include "compat/enum-bitset.hpp"
#include "src/world.hpp"
#include "editor.hpp"
#include "tests.hpp"
#include "draw/wireframe-meshes.hpp"
#include "floormat/main.hpp"
#include <Magnum/ImGuiIntegration/Context.h>

namespace floormat {

app::app(fm_settings&& opts) :
    M{floormat_main::create(*this, move(opts))},
    _imgui{InPlaceInit, NoCreate},
    _wireframe{InPlaceInit},
    _tests{tests_data_::make()},
    _editor{InPlaceInit, this},
    keys_{InPlaceInit, 0u},
    key_modifiers{}
{
    reset_world_post();
    auto& w = M->world();
    constexpr chunk_coords_ coord{0, 0, 0};
    maybe_initialize_chunk_(coord, w[coord]);
    reset_camera_offset();
    M->set_render_vobjs(_render_vobjs);
    reserve_inspector_array();
}

app::~app()
{
    reset_world_pre();
}

} // namespace floormat
