#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/sysexits.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "editor.hpp"
#include "src/anim-atlas.hpp"
#include "src/critter.hpp"
#include "src/world.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "loader/loader.hpp"
#include <cstdlib>
#include <cstring>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Move.h>

namespace floormat {

Optional<struct point> cursor_state::point() const
{
    if (tile)
        return {InPlaceInit, *tile, *subpixel};
    else
        return {};
}

floormat_main& app::main() { return *M; }
const cursor_state& app::cursor_state() { return cursor; }


shared_ptr_wrapper<critter> app::ensure_player_character(world& w)
{
    if (_character_id)
    {
        std::shared_ptr<critter> tmp;
        if (auto C = w.find_object(_character_id); C && C->type() == object_type::critter)
        {
            auto ptr = std::static_pointer_cast<critter>(C);
            return {ptr};
        }
    }
    _character_id = 0;

    auto id = (object_id)-1;

    shared_ptr_wrapper<critter> ret;

    for (const auto& [coord, c] : w.chunks())
    {
        for (const auto& e_ : c.objects())
        {
            const auto& e = *e_;
            if (e.type() == object_type::critter)
            {
                const auto& C = static_cast<const critter&>(e);
                if (C.playable)
                {
                    id = std::min(id, C.id);
                    ret.ptr = std::static_pointer_cast<critter>(e_);
                }
            }
        }
    }

    if (id != (object_id)-1)
        _character_id = id;
    else
    {
        critter_proto cproto;
        cproto.name = "Player"_s;
        cproto.playable = true;
        ret.ptr = w.make_object<critter>(w.make_id(), global_coords{}, cproto);
        _character_id = ret.ptr->id;
    }
    fm_debug_assert(ret.ptr);
    return shared_ptr_wrapper<critter>{ret};
}

void app::reset_world(class world&& w_)
{
    if (!M)
        return;

    _editor->on_release();
    _editor->clear_selection();
    kill_popups(true);
    tested_light_chunk = {};
    tests_reset_mode();

    clear_keys();
    const auto pixel = cursor.pixel;
    cursor = {};
    _character_id = 0;
    _render_vobjs = true;
    _render_all_z_levels = true;

    auto& w = M->reset_world(Utility::move(w_));
    w.collect(true);
    ensure_player_character(w);
    update_cursor_tile(pixel);
}

int app::exec()
{
    return M->exec();
}

static const char* const true_values[]  = { "1", "true", "yes", "y", "Y", "on", "ON", "enable", "enabled", };
static const char* const false_values[] = { "0", "false", "no", "n", "N", "off", "OFF", "disable", "disabled", };

template<typename T, typename U>
static inline bool find_arg(const T& list, const U& value) {
    for (const auto& x : list)
        if (x == value)
            return true;
    return false;
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
        if (ret != 2 || (size_t)n != str.size() || Vector2ui(size).product() == 0)
        {
            Error{} << "invalid --geometry argument '%s'" << str;
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

    struct app* A = new app{Utility::move(opts)};
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
