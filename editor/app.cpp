#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/sysexits.hpp"
#include "compat/split-string.hpp"
#include "editor.hpp"
#include "src/anim-atlas.hpp"
#include "src/critter.hpp"
#include "src/world.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "loader/loader.hpp"
#include "editor/pgo-scenes.hpp"
#include <cr/StringIterable.h>
#include <cr/Arguments.h>
#include <cr/GrowableArray.h>
#include <algorithm>
#include <ranges>

namespace floormat {

namespace ranges = std::ranges;

namespace {

constexpr const char* const true_values[]  = { "1", "true", "yes", "y", "Y", "on", "ON", "enable", "enabled", };
constexpr const char* const false_values[] = { "0", "false", "no", "n", "N", "off", "OFF", "disable", "disabled", };

template<typename T, typename U>
bool find_arg(const T& list, const U& value) {
    for (const auto& x : list)
        if (x == value)
            return true;
    return false;
}

bool parse_bool(StringView name, const Corrade::Utility::Arguments& args)
{
    auto str = args.value<StringView>(name);
    if (find_arg(true_values, str))
        return true;
    else if (find_arg(false_values, str))
        return false;
    ERR_nospace << "invalid --" << name << " argument '" << str << "': should be 'true' or 'false'";
    std::exit(EX_USAGE);
}

uint32_t parse_uint(StringView name, const Corrade::Utility::Arguments& args)
{
    auto str = args.value<StringView>(name);
    uint32_t value = 0;
    int n = 0;
    if (std::sscanf(str.data(), "%u%n", &value, &n) != 1 || (size_t)n != str.size())
    {
        ERR_nospace << "invalid --" << name << " argument '" << str << "': should be a number";
        std::exit(EX_USAGE);
    }
    return value;
}

driver_mode parse_driver(const Corrade::Utility::Arguments& args)
{
    auto str = args.value<StringView>("driver");
    if (str == "off"_s)
        return driver_mode::off;
    else if (str == "all"_s)
        return driver_mode::all;
    else if (str == "coverage"_s)
        return driver_mode::coverage;
    else if (str == "profile"_s)
        return driver_mode::profile;
    ERR_nospace << "invalid --driver argument '" << str << "': should be off, all, coverage or profile";
    std::exit(EX_USAGE);
}

} // namespace

Optional<struct point> cursor_state::point() const
{
    if (tile)
        return {InPlaceInit, *tile, *subpixel};
    else
        return {};
}

floormat_main& app::main() { return *M; }
const cursor_state& app::cursor_state() { return cursor; }

bptr<critter> app::ensure_player_character(world& w)
{
    return w.ensure_player_character(_character_id);
}

void app::reset_world()
{
    if (M)
        reset_world(world{});
}

void app::reset_world_pre()
{
    auto& w = M->world();
    w.finish_scripts();
    _editor->on_release();
    //_editor->clear_selection();
    kill_popups(true);
    tested_light_chunk = {};
    tests_reset_mode();
    clear_keys();
    _character_id = 0;
    _render_vobjs = true;
    M->set_render_vobjs(_render_vobjs);
    _render_all_z_levels = true;
    _timestamp = 0;
    const auto pixel = cursor.pixel;
    cursor = {};
    cursor.pixel = pixel;
}

void app::reset_world_post()
{
    auto& w = M->world();
    w.collect(true);
    ensure_player_character(w);
    update_cursor_tile(cursor.pixel);
    w.init_scripts();
    M->reset_fps();
}

void app::reset_world(class world&& wʹ)
{
    fm_assert(M);
    reset_world_pre();
    M->reset_world(move(wʹ));
    reset_world_post();
}

int app::exec()
{
    return M->exec();
}

fm_settings app::parse_cmdline(int argc, const char* const* const argv)
{
    fm_settings opts;
    Corrade::Utility::Arguments args{};
    args.addSkippedPrefix("magnum")
        .addOption("vsync", "1").setFromEnvironment("vsync", "FLOORMAT_VSYNC").setHelp("vsync", "vertical sync", "true|false")
        .addOption('g', "geometry", "").setHelp("geometry", "width x height, e.g. 1024x768", "WxH")
        .addOption("window", "windowed").setFromEnvironment("window", "FLOORMAT_WINDOW_MODE").setHelp("window", "window mode", "windowed|fullscreen|borderless")
        .addOption("driver", "off").setHelp("driver", "run driver scenes, then quit", "off|all|coverage|profile")
        .addOption("fixed-framerate", "0").setHelp("fixed-framerate", "feed update() a constant dt", "HZ")
        .addOption("driver-repeat", "1").setHelp("driver-repeat", "run the scene table N times", "N")
        .addOption("driver-scenes", "all").setHelp("driver-scenes", "scene names, or list|all|none", "a,b,c")
        .parse(argc, argv);
    opts.vsync = parse_bool("vsync", args);
    opts.driver = parse_driver(args);
    // Otherwise the scenes measure the swap interval. The raycast sweep alone yields 512 times
    // and the walk 1022, which at 60 Hz is time spent in the driver doing nothing.
    if (opts.driver != driver_mode::off)
        opts.vsync = false;
    opts.fixed_framerate = parse_uint("fixed-framerate", args);
    opts.driver_repeat = parse_uint("driver-repeat", args);
    {
        const auto scenes = app::scenes();
        const auto driver_scenes = split_string(args.value<StringView>("driver-scenes"), ',');
        Array<StringView> output; arrayReserve(output, 16);
        const auto pushnew = [&](StringView s) {
            if (!ranges::contains(output, s))
                arrayAppend(output, s);
        };
        for (StringView name : driver_scenes)
        {
            if (name == "help"_s || name == "list"_s)
            {
                for (const auto& s : scenes)
                    std::printf("%-16s%s\n", s.name.exceptPrefix("scene_"_s).data(),
                                s.mode == driver_mode::coverage ? "coverage" : "profile");
                std::fflush(stdout);
                // Not std::exit(): a world is live by this point, and skipping its teardown trips
                // the RTree pool's leak assert. quit() returns through Sdl2Application::exit.
                std::exit(0);
            }
            else if (name == "none"_s)
                arrayClear(output);
            else if (name == "all")
            {
                arrayClear(output);
                for (const auto& s : scenes)
                    pushnew(s.name.exceptPrefix("scene_"_s));
            }
            else
            {
                if (ranges::contains(scenes, name, [&](auto&& s) { return s.name.exceptPrefix("scene_"_s); }))
                    pushnew(name);
                else
                {
                    auto err = ERR_nospace;
                    err << "invalid --driver-scenes name '" << name << "', known scenes:";
                    for (const auto& s : scenes)
                        err << " " << s.name.exceptPrefix("scene_"_s);
                    std::exit(EX_USAGE);
                }
            }
        }
        opts.driver_scenes = ","_s.join(output);
    }
    if (opts.driver_repeat == 0)
    {
        ERR_nospace << "--driver-repeat must be at least 1";
        std::exit(EX_USAGE);
    }
    if (auto str = args.value<StringView>("geometry"))
    {
        Vector2us size;
        int n = 0, ret = std::sscanf(str.data(), "%hux%hu%n", &size.x(), &size.y(), &n);
        if (ret != 2 || (size_t)n != str.size() || Vector2ui(size).product() == 0)
        {
            ERR_nospace << "invalid --geometry argument '" << str << "'";
            std::exit(EX_USAGE);
        }
        else
            opts.resolution = Vector2i(size);
    }
    if (auto str = args.value<StringView>("window");
        str == "fullscreen")
    {
        opts.fullscreen = true;
        opts.resizable = false;
    }
    else if (str == "borderless")
    {
        opts.borderless = true;
        opts.resizable = false;
    }
    else if (str == "fullscreen-desktop")
    {
        opts.fullscreen_desktop = true;
        opts.resizable = false;
    }
    else if (str == "maximize" || str == "maximized")
        opts.maximized = true;
    else if (str == "windowed")
        (void)0;
    else
    {
        ERR_nospace << "invalid display mode '" << str << "'";
        std::exit(EX_USAGE);
    }
    return opts;
}

int app::run_from_argv(const int argc, const char* const* const argv)
{
    auto opts = parse_cmdline(argc, argv);
    int ret;
    //auto [argv2, argc2] = make_argv_for_magnum(opts, argv ? argv[0] : "floormat");
    opts.argv = argv;
    opts.argc = argc;

    struct app* A = new app{move(opts)};
    floormat_main* M = A->M;
    fm_assert(M != nullptr);
    ret = A->exec();
    loader.destroy();
    delete A;
    delete M;
    return ret;
}

} // namespace floormat

int main(int argc, char** argv)
{
    floormat::floormat_main::init_pre();

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
