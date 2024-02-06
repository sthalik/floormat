#include "app.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Path.h>

namespace floormat {

#define save_dir "save"
#define quicksave_file save_dir "/" "quicksave.dat"
#define quicksave_tmp save_dir "/" "quicksave.tmp"

static bool ensure_save_directory()
{
    auto dir = Path::join(loader.TEMP_PATH, save_dir);
    if (Path::make(save_dir))
    {
        fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt"_s)));
        return true;
    }
    else
    {
        fm_warn("failed to create save directory '%s'", save_dir);
        return false;
    }
}

void app::do_quicksave()
{
    auto file = Path::join(loader.TEMP_PATH, quicksave_file);
    auto tmp = Path::join(loader.TEMP_PATH, quicksave_tmp);
    if (!ensure_save_directory())
        return;
    auto& world = M->world();
    world.collect(true);
    if (Path::exists(tmp))
        Path::remove(tmp);
    fputs("quicksave... ", stderr); fflush(stderr);
    world.serialize(tmp);
    Path::move(tmp, file);
    fputs("done\n", stderr); fflush(stderr);
}

void app::do_quickload()
{
    auto file = Path::join(loader.TEMP_PATH, quicksave_file);
    if (!ensure_save_directory())
        return;
    if (!Path::exists(file))
    {
        fm_warn("no quicksave");
        return;
    }
    fputs("quickload... ", stderr); fflush(stderr);
    reset_world(world::deserialize(file, loader_policy::warn));
    fputs("done\n", stderr); fflush(stderr);
}

void app::do_new_file()
{
    reset_world();
    auto& w = M->world();
    maybe_initialize_chunk_(chunk_coords_{}, w[chunk_coords_{}]);
}

} // namespace floormat
