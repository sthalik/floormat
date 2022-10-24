#include "app.hpp"
#include "compat/assert.hpp"
#include "main/floormat-main.hpp"
#include "main/floormat.hpp"
#include "src/loader.hpp"
#include <Corrade/Utility/Arguments.h>

namespace floormat {

app::app() :
    M{floormat_main::create(*this, {})},
    _floor1{loader.tile_atlas("floor-tiles", {44, 4})},
    _floor2{loader.tile_atlas("metal1", {2, 2})},
    _wall1{loader.tile_atlas("wood2", {1, 1})},
    _wall2{loader.tile_atlas("wood1", {1, 1})}
{
}

app::~app() // NOLINT(modernize-use-equals-default)
{
}

int app::exec()
{
    return M->exec();
}

static const char* const true_values[]  = { "1", "true", "yes", "y", "Y", "on", "ON", };
static const char* const false_values[] = { "0", "false", "no", "n", "N", "off", "OFF", };
static const char* const maybe_values[] = { "maybe", "m", "M", "default", };

template<typename T, typename U>
static inline bool find_arg(const T& list, const U& value) {
    return std::find_if(std::cbegin(list), std::cend(list),
                        [&](const auto& x) { return x == value; }) != std::cend(list);
}

static fm_tristate parse_tristate(StringView name, StringView str, fm_tristate def)
{
    if (find_arg(true_values, str))
        return fm_tristate::on;
    else if (find_arg(false_values, str))
        return fm_tristate::off;
    else if (find_arg(maybe_values, str))
        return fm_tristate::maybe;

    fm_warn("invalid '%s' argument '%s': should be true, false or default", name.data(), str.data());
    return def;
}

static bool parse_bool(StringView name, StringView str, bool def)
{
    if (find_arg(true_values, str))
        return true;
    else if (find_arg(false_values, str))
        return false;
    fm_warn("invalid '%s' argument '%s': should be true or false", name.data(), str.data());
    return def;
}

int app::run_from_argv(const int argc, const char* const* const argv)
{
    fm_settings opts;
    {
        Corrade::Utility::Arguments args{};
        args.addOption("vsync", "m")
            .addOption("gpu-validation", "m")
            .addOption("msaa", "1")
            .parse(argc, argv);
        opts.vsync = parse_tristate("--vsync", args.value<StringView>("vsync"), opts.vsync);
        opts.msaa  = parse_bool("--msaa", args.value<StringView>("msaa"), opts.msaa);
        {
            auto str = args.value<StringView>("gpu-validation");
            if (str == "no-error" || str == "NO-ERROR")
                opts.gpu_debug = fm_gpu_debug::no_error;
            else if (str == "robust" || str == "robust")
                opts.gpu_debug = fm_gpu_debug::robust;
            else switch (parse_tristate("--gpu-validation", args.value<StringView>("gpu-validation"), fm_tristate::maybe))
                 {
                 default:
                 case fm_tristate::on: opts.gpu_debug = fm_gpu_debug::on; break;
                 case fm_tristate::off: opts.gpu_debug = fm_gpu_debug::off; break;
                 }
        }
    }

    int ret;
    Pointer<floormat_main> ptr;
    {
        app application;
        ret = application.exec();
        ptr = std::move(application.M);
    }
    loader_::destroy();
    return ret;
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

} // namespace floormat

int main(int argc, char** argv)
{
    return floormat::app::run_from_argv(argc, argv);
}
