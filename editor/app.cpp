#include "app.hpp"
#include "compat/assert.hpp"
#include "main/floormat-main.hpp"
#include "main/floormat.hpp"
#include "src/loader.hpp"
#include <Corrade/Utility/Arguments.h>

namespace floormat {

app::app() :
    M{ floormat_main::create(*this, {})},
    _dummy{M->register_debug_callback()},
    _floor1{loader.tile_atlas("floor-tiles", {44, 4})},
    _floor2{loader.tile_atlas("metal1", {2, 2})},
    _wall1{loader.tile_atlas("wood2", {1, 1})},
    _wall2{loader.tile_atlas("wood1", {1, 1})}
{
}

app::~app()
{
    loader_::destroy();
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
    fm_options opts;
    {
        Corrade::Utility::Arguments args{};
        args.addOption("vsync", "default")
            .addOption("gpu-validation", "true")
            .parse(argc, argv);
        opts.vsync = parse_tristate("--vsync", args.value<StringView>("vsync"), opts.vsync);
    }
    app application;
    return application.exec();
}

} // namespace floormat
