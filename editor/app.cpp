#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/sysexits.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "loader/loader.hpp"
#include "world.hpp"
#include "src/anim-atlas.hpp"
#include "src/character.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <array>
#include <utility>
#include <algorithm>
#include <Corrade/Containers/StringIterable.h>
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
    reset_world();
    auto& w = M->world();
    constexpr chunk_coords coord{0, 0};
    maybe_initialize_chunk_(coord, w[coord]);
    reset_camera_offset();
    inspectors.reserve(16);
}

app::~app() = default;

void app::reset_world()
{
    reset_world(floormat::world{});
}

void app::ensure_player_character(world& w)
{
    if (_character_id)
        if (auto C = w.find_entity(_character_id); C && C->type == entity_type::character)
            return;
    _character_id = 0;

    auto id = (std::uint64_t)-1;

    for (const auto& [coord, c] : w.chunks())
    {
        for (const auto& e_ : c.entities())
        {
            const auto& e = *e_;
            if (e.type == entity_type::character)
            {
                const auto& C = static_cast<const character&>(e);
                if (C.playable)
                    id = std::min(id, C.id);
            }
        }
    }

    if (id != (std::uint64_t)-1)
        _character_id = id;
    else
    {
        character_proto cproto;
        cproto.name = "Player"_s;
        cproto.playable = true;
        _character_id = w.make_entity<character>(w.make_id(), global_coords{}, cproto)->id;
    }
}

void app::reset_world(struct world&& w)
{
    _popup_target = {};
    _character_id = 0;
    if (!M)
        return;
    auto& w2 = M->reset_world(std::move(w));
    w2.collect(true);
    ensure_player_character(w2);
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

static bool parse_bool(StringView name, const Corrade::Utility::Arguments& args)
{
    StringView str = args.value<StringView>(name);
    if (find_arg(true_values, str))
        return true;
    else if (find_arg(false_values, str))
        return false;
    Error{Error::Flag::NoSpace} << "invalid --" << name << " argument '" << str << "': should be 'true' or 'false'";
    std::exit(EX_USAGE);
}

fm_settings app::parse_cmdline(int argc, const char* const* const argv)
{
    fm_settings opts;
    Corrade::Utility::Arguments args{};
    args.addSkippedPrefix("magnum")
        .addOption("vsync", "1").setFromEnvironment("vsync", "FLOORMAT_VSYNC").setHelp("vsync", "vertical sync", "true|false")
        .addOption('g', "geometry", "").setHelp("geometry", "width x height, e.g. 1024x768", "WxH")
        .addOption("window", "windowed").setFromEnvironment("window", "FLOORMAT_WINDOW_MODE").setHelp("window", "window mode", "windowed|fullscreen|borderless")
        .parse(argc, argv);
    opts.vsync = parse_bool("vsync", args);
    if (auto str = args.value<StringView>("geometry"))
    {
        Vector2us size;
        int n = 0, ret = std::sscanf(str.data(), "%hux%hu%n", &size.x(), &size.y(), &n);
        if (ret != 2 || (std::size_t)n != str.size() || Vector2ui(size).product() == 0)
        {
            Error{} << "invalid --geometry argument '%s'" << str;
            std::exit(EX_USAGE);
        }
        else
            opts.resolution = Vector2i(size);
    }
    if (auto str = args.value<StringView>("window");
        str == "fullscreen")
        opts.fullscreen = true, opts.resizable = false;
    else if (str == "borderless")
        opts.borderless = true, opts.resizable = false;
    else if (str == "fullscreen-desktop")
        opts.fullscreen_desktop = true, opts.resizable = false;
    else if (str == "maximize" || str == "maximized")
        opts.maximized = true;
    else if (str == "windowed")
        (void)0;
    else
    {
        Error{Error::Flag::NoSpace} << "invalid display mode '" << str << "'";
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
