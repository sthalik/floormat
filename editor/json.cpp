#include "app.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/world.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat {

#define FM_SAVE_BINARY

#ifdef FM_SAVE_BINARY
#define quicksave_file save_dir "/" "quicksave.dat"
#else
#define quicksave_file save_dir "/" "quicksave.json"
#endif

#define save_dir "../save"
#define quicksave_tmp quicksave_file ".tmp"

namespace Path = Corrade::Utility::Path;
using std::filesystem::path;

static bool ensure_save_directory()
{
    if (Path::make(save_dir))
        return true;
    else
    {
        fm_warn("failed to create save directory '%s'", save_dir);
        return false;
    }
}

void app::do_quicksave()
{
    if (!ensure_save_directory())
        return;
    auto& world = M->world();
    world.collect(true);
    if (Path::exists(quicksave_tmp))
        Path::remove(quicksave_tmp);
    fputs("quicksave...", stderr); fflush(stderr);
#if 0
#ifdef FM_SAVE_BINARY
    json_helper::to_binary(world, quicksave_tmp);
#else
    json_helper::to_json(world, quicksave_tmp);
#endif
#endif
    Path::move(quicksave_tmp, quicksave_file);
    fputs(" done\n", stderr); fflush(stderr);
}

void app::do_quickload()
{
    ensure_save_directory();
    if (!Path::exists(quicksave_file))
    {
        fm_warn("no quicksave");
        return;
    }
    auto& world = M->world();
    fputs("quickload...", stderr); fflush(stderr);
#if 0
#ifdef FM_SAVE_BINARY
    world = json_helper::from_binary<struct world>(quicksave_file);
#else
    world = json_helper::from_json<struct world>(quicksave_file);
#endif
#endif
    fputs(" done\n", stderr); fflush(stderr);
}

} // namespace floormat
