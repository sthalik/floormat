#include "main-impl.hpp"
#include "compat/fpu.hpp"
#include "src/path-search.hpp"
#include <Corrade/Containers/GrowableArray.h>

namespace floormat {

main_impl::main_impl(floormat_app& app, fm_settings&& se, int& argc, char** argv) noexcept :
    Platform::Sdl2Application{Arguments{argc, argv},
                              make_conf(se), make_gl_conf(se)},
    s{std::move(se)}, app{app}, _shader{_tuc}
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
    set_fp_mask();
    arrayReserve(_clickable_scenery, 128);
    timeline.start();
}

class world& main_impl::reset_world(class world&& w) noexcept
{
    arrayResize(_clickable_scenery, 0);
    _world = std::move(w);
    return _world;
}

} // namespace floormat