#include "app.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/world.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat {

#define save_dir "../save"
#define quicksave_file save_dir "/" "quicksave.json"
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
    json_helper::to_json(world, quicksave_tmp);
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
    world = json_helper::from_json<struct world>(quicksave_file);
    fputs(" done\n", stderr); fflush(stderr);
}

} // namespace floormat
