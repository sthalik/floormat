#include "app.hpp"
#include "compat/assert.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "loader/loader.hpp"
#include "world.hpp"
#include "src/anim-atlas.hpp"
#include <algorithm>
#include <Corrade/Utility/Arguments.h>

namespace floormat {

app::app(fm_settings&& opts) :
    M{floormat_main::create(*this, std::move(opts))},
    _floor1{loader.tile_atlas("floor-tiles", {44, 4}, pass_mode::pass)},
    _floor2{loader.tile_atlas("metal1", {2, 2}, pass_mode::pass)},
    _wall1{loader.tile_atlas("wood2", {2, 1}, pass_mode::blocked)},
    _wall2{loader.tile_atlas("wood1", {2, 1}, pass_mode::blocked)},
    _door{loader.anim_atlas("door-close", loader.SCENERY_PATH)},
    _table{loader.anim_atlas("table", loader.SCENERY_PATH)},
    _control_panel(loader.anim_atlas("control-panel", loader.SCENERY_PATH))
{
    world& w = M->world();
    chunk_coords coord{0 ,0};
    maybe_initialize_chunk_(coord, w[coord]);
    reset_camera_offset();
}

app::~app()
{
    void();
}

int app::exec()
{
    return M->exec();
}

static const char* const true_values[]  = { "1", "true", "yes", "y", "Y", "on", "ON", "enable", "enabled", };
static const char* const false_values[] = { "0", "false", "no", "n", "N", "off", "OFF", "disable", "disabled", };

template<typename T, typename U>
static inline bool find_arg(const T& list, const U& value) {
    return std::find_if(std::cbegin(list), std::cend(list), [&](const auto& x) { return x == value; }) != std::cend(list);
}

static bool parse_bool(StringView name, const Corrade::Utility::Arguments& args, bool def)
{
    StringView str = args.value<StringView>(name);
    if (find_arg(true_values, str))
        return true;
    else if (find_arg(false_values, str))
        return false;
    fm_warn("invalid '--%s' argument '%s': should be true or false", name.data(), str.data());
    return def;
}

[[maybe_unused]]
static int atoi_(const char* str)
{
    bool negative = false;

    switch (*str)
    {
    case '+': ++str; break;
    case '-': ++str; negative = true; break;
    }

    int result = 0;
    for (; *str >= '0' && *str <= '9'; ++str)
    {
        int digit = *str - '0';
        result *= 10;
        result -= digit; // calculate in negatives to support INT_MIN, LONG_MIN,..
    }

    return negative ? result : -result;
}

fm_settings app::parse_cmdline(int argc, const char* const* const argv)
{
    fm_settings opts;
    Corrade::Utility::Arguments args{};
    args.addOption("vsync", "1").setFromEnvironment("vsync", "FLOORMAT_VSYNC").setHelp("vsync", "", "true|false")
        .addOption("gpu-debug", "1").setFromEnvironment("gpu-debug", "FLOORMAT_GPU_DEBUG").setHelp("gpu-debug", "", "robust|on|off|no-error")
        .addOption("geometry", "").setHelp("geometry", "width x height, e.g. 1024x768", "WxH")
        .addSkippedPrefix("magnum")
        .parse(argc, argv);
    opts.vsync = parse_bool("vsync", args, opts.vsync);
    if (auto str = args.value<StringView>("gpu-debug"); str == "no-error"_s)
        opts.gpu_debug = fm_gpu_debug::no_error;
    else if (str == "robust"_s || str == "full"_s)
        opts.gpu_debug = fm_gpu_debug::robust;
    else
        opts.gpu_debug = parse_bool("gpu-debug", args, opts.gpu_debug > fm_gpu_debug::off)
                         ? fm_gpu_debug::on
                         : fm_gpu_debug::off;
    if (auto str = args.value<StringView>("geometry"); !str.isEmpty())
    {
        Vector2us size;
        int n = 0, ret = std::sscanf(str.data(), "%hux%hu%n", &size.x(), &size.y(), &n);
        if (ret != 2 || (std::size_t)n != str.size() || Vector2ui(size).product() == 0)
            fm_warn("invalid --geom argument '%s'", str.data());
        else
            opts.resolution = Vector2i(size);
    }
    opts.argc = argc;
    opts.argv = argv;
    return opts;
}

int app::run_from_argv(const int argc, const char* const* const argv)
{
    auto opts = parse_cmdline(argc, argv);
    int ret;
    Pointer<floormat_main> ptr;
    {
        app application{std::move(opts)};
        ret = application.exec();
        ptr = std::move(application.M);
        (void)ptr;
    }
    loader_::destroy();
    return ret;
}

} // namespace floormat

int main(int argc, char** argv)
{
#ifdef _WIN32
    floormat::app::set_dpi_aware();
#endif
    return floormat::app::run_from_argv(argc, argv);
}

#ifdef _MSC_VER
#include <cstdlib> // for __arg{c,v}
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wmain"
#endif
extern "C" int __stdcall WinMain(void*, void*, void*, int);

extern "C" int __stdcall WinMain(void*, void*, void*, int)
{
    return main(__argc, __argv);
}
#ifdef __clang__
#    pragma clang diagnostic pop
#endif
#endif
