#include "main-impl.hpp"
#include "compat/fpu.hpp"
#include "src/search-astar.hpp"
#include "src/search.hpp"
#include "src/chunk.hpp"
#include <cmath>
#include <algorithm>
#include <Corrade/Containers/GrowableArray.h>

namespace floormat {

main_impl::main_impl(floormat_app& app, fm_settings&& se, int& argc, char** argv) noexcept :
    Platform::Sdl2Application{Arguments{argc, argv},
                              make_conf(se), make_gl_conf(se)},
    s{move(se)}, app{app}, _shader{_tuc}
{
    if (s.vsync)
    {
        (void)setSwapInterval(1);
        if (const auto list = GL::Context::current().extensionStrings();
            std::find(list.cbegin(), list.cend(), "EXT_swap_control_tear") != list.cend())
            (void)setSwapInterval(-1);
    }
    else
        (void)setSwapInterval(0);
    maybe_enable_clipcontrol_zero_to_one();
    set_fp_mask();
    arrayReserve(_clickable_scenery, 128);
    timeline = Time::now();
}

class world& main_impl::reset_world(class world&& w) noexcept
{
    arrayResize(_clickable_scenery, 0);

    for (auto& [_, cʹ] : _world.chunks())
        for (const auto& eʹ : cʹ.objects())
            fm_assert_equal(uint32_t{2}, eʹ.use_count());

    _world = move(w);
    _first_frame = true;
    return _world;
}

} // namespace floormat
