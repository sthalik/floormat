#include "app.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "compat/format.hpp"
#include "imgui-raii.hpp"
#include <cstdio>
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

void app::load_world_file(StringView path)
{
    const auto name = path.hasPrefix(loader.TEMP_PATH)
                      ? path.exceptPrefix(loader.TEMP_PATH.size()) : path;
    std::fputs("load '", stderr);
    std::fwrite(name.data(), name.size(), 1, stderr);
    std::fputs("'... ", stderr);
    std::fflush(stderr);
    reset_world(world::deserialize(path, loader_policy::warn));
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
                char label[256];
                snformat(label, "{}/{}"_cf, dir_name, file);
                if (ImGui::Selectable(label))
                {
                    _show_load_pane = false;
                    load_world_file(Path::join(dir, file));
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
