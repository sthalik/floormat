#include "main-impl.hpp"
#include "compat/arch.hpp"
#include "src/search-astar.hpp"
#include "src/search.hpp"
#include "src/chunk.hpp"
#include "src/object-storage.inl"
#include "compat/strerror.hpp"
#include <algorithm>
#include <cerrno>
#include <cr/GrowableArray.h>

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
    if (!s.log_frame_times.isEmpty())
    {
        _frame_times_file = std::fopen(s.log_frame_times.data(), "ab");
        if (!_frame_times_file)
        {
            int error = errno;
            char errbuf[128];
            fm_abort("fopen(\"%s\", \"a\"): %s", s.log_frame_times.data(), get_error_string(errbuf, error).data());
        }
    }
    timeline = Time::now();
}

class world& main_impl::reset_world(class world&& w) noexcept
{
    arrayResize(_clickable_scenery, 0);

    for (auto& cʹ : _world.chunks())
        for (const auto& eʹ : cʹ.objects())
            fm_assert_equal(uint32_t{2}, eʹ.use_count());

    _world = move(w);
    _world.chunk_table_prepare_frame();
    _first_frame = true;
    return _world;
}

} // namespace floormat
