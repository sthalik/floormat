#include "app.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "compat/format.hpp"
#include "compat/sysexits.hpp"
#include "imgui-raii.hpp"
#include <cstdio>
#include <ctime>
#include <cr/String.h>
#include <cr/StringView.h>
#include <cr/Array.h>
#include <cr/Optional.h>
#include <cr/Path.h>

namespace floormat {

using namespace floormat::imgui;

#define save_dir "save"
#define driver_save_dir "driver-saves"
#define quicksave_file save_dir "/" "quicksave.dat"
#define quicksave_tmp save_dir "/" "quicksave.tmp"

namespace {

bool ensure_directory(StringView name)
{
    auto dir = Path::join(loader.TEMP_PATH, name);
    if (Path::make(dir))
    {
        fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt"_s)));
        return true;
    }
    else
    {
        fm_warn("failed to create directory '%s'", dir.data());
        return false;
    }
}

StringView relative_to_temp_path(StringView path)
{
    return path.hasPrefix(loader.TEMP_PATH) ? path.exceptPrefix(loader.TEMP_PATH.size()) : path;
}

} // namespace

void app::do_quicksave()
{
    auto file = Path::join(loader.TEMP_PATH, quicksave_file);
    auto tmp = Path::join(loader.TEMP_PATH, quicksave_tmp);
    if (!ensure_directory(StringView{save_dir}))
        return;
    auto& world = M->world();
    world.collect(true);
    if (Path::exists(tmp))
        Path::remove(tmp);
    std::fputs("quicksave... ", stderr); std::fflush(stderr);
    world.serialize(tmp);
    Path::move(tmp, file);
    std::fputs("done\n", stderr); std::fflush(stderr);
}

void app::do_quickload()
{
    auto file = Path::join(loader.TEMP_PATH, quicksave_file);
    if (!ensure_directory(StringView{save_dir}))
        return;
    if (!Path::exists(file))
    {
        fm_warn("no quicksave");
        return;
    }
    load_world_file(file);
}

void app::load_world_file(StringView path)
{
    // Re-checked: --load-game resolves the path in parse_cmdline, long before this call.
    if (!Path::exists(path) || Path::isDirectory(path))
    {
        WARN_nospace << "no such file '" << relative_to_temp_path(path) << "'";
        return;
    }
    const auto name = relative_to_temp_path(path);
    std::fputs("load '", stderr);
    std::fwrite(name.data(), name.size(), 1, stderr);
    std::fputs("'... ", stderr);
    std::fflush(stderr);
    reset_world(world::deserialize(path, loader_policy::warn));
    std::fputs("done\n", stderr); std::fflush(stderr);
}

String app::resolve_load_game_path(StringView name)
{
    // Path::join treats the filename as absolute only with forward slashes.
    const auto file = Path::fromNativeSeparators(name);
    auto path = Path::join(Path::join(loader.TEMP_PATH, StringView{save_dir}), file);
    if (!Path::exists(path) || Path::isDirectory(path))
    {
        // Path::join returns an absolute `file` unchanged regardless of base: a no-op here.
        auto fallback = Path::join(loader.startup_directory(), file);
        if (Path::exists(fallback) && !Path::isDirectory(fallback))
            path = move(fallback);
    }
    if (!Path::exists(path) || Path::isDirectory(path))
    {
        ERR_nospace << "--load-game: no such file '" << relative_to_temp_path(path) << "'";
        std::exit(EX_USAGE);
    }
    return path;
}

void app::driver_save_world(uint32_t scene_number, StringView scene_name, bool is_post)
{
    if (!ensure_directory(StringView{driver_save_dir}))
        return;
    char name[96];
    // A StringView of a char[N] buffer spans all N bytes, not up to the NUL.
    const auto len = snformat(name, "{}/driver-{:02}_{}-{}.dat"_cf, StringView{driver_save_dir},
                              scene_number, scene_name, is_post ? "post"_s : "pre"_s);
    fm_assert(len < sizeof name);
    const auto path = Path::join(loader.TEMP_PATH, StringView{name, len, StringViewFlag::NullTerminated});
    auto& w = M->world();
    w.collect(true);
    w.serialize(path);
}

void app::do_load_file()
{
    _show_load_pane = !_show_load_pane;
}

void app::draw_load_pane(float main_menu_height)
{
    if (!_show_load_pane)
        return;

    const auto dpi = M->dpi_scale();
    ImGui::SetNextWindowPos({60*dpi.x(), (main_menu_height + 1) * dpi.y()}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({460*dpi.x(), 420*dpi.y()}, ImGuiCond_Appearing);

    bool is_open = true;
    if (auto b = begin_window("Load save file"_s, &is_open))
    {
        using LF = Path::ListFlag;
        constexpr StringView dirs[] = { save_dir ""_s, driver_save_dir ""_s };
        for (const auto& dir_name : dirs)
        {
            auto dir = Path::join(loader.TEMP_PATH, dir_name);
            auto files = Path::list(dir, LF::SkipDirectories|LF::SkipSpecial|LF::SkipDotAndDotDot|
                                         LF::SortAscending);
            ImGui::SeparatorText(dir_name.data());
            if (!files)
                continue;
            for (const StringView file : *files)
            {
                if (!file.hasSuffix(".dat"_s))
                    continue;
                auto path = Path::join(dir, file);
                char date[24] = "";
                if (const auto ns = Path::lastModification(path))
                {
                    const auto t = (std::time_t)(*ns / 1000000000);
                    if (const auto* tm = std::localtime(&t))
                        std::strftime(date, sizeof date, "%Y-%m-%d %H:%M", tm);
                }
                char label[256];
                snformat(label, "{}"_cf, file);
                const bool clicked = ImGui::Selectable(label);
                ImGui::SameLine(300*dpi.x());
                ImGui::TextUnformatted(date);
                if (clicked)
                {
                    _show_load_pane = false;
                    load_world_file(path);
                    return;
                }
            }
        }
    }

    if (!is_open)
        _show_load_pane = false;
}

void app::do_new_file()
{
    reset_world();
    auto& w = M->world();
    maybe_initialize_chunk_({}, w[{}]);
}

} // namespace floormat
