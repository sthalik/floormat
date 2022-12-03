#include "app.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat {

#define save_dir "../save"
#define quicksave_file save_dir "/" "quicksave.dat"
#define quicksave_tmp save_dir "/" "quicksave.tmp"

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
    fputs("quicksave... ", stderr); fflush(stderr);
    world.serialize(quicksave_tmp);
    Path::move(quicksave_tmp, quicksave_file);
    fputs("done\n", stderr); fflush(stderr);
}

void app::do_quickload()
{
    if (!ensure_save_directory())
        return;
    if (!Path::exists(quicksave_file))
    {
        fm_warn("no quicksave");
        return;
    }
    fputs("quickload... ", stderr); fflush(stderr);
    M->world() = world::deserialize(quicksave_file);
    fputs("done\n", stderr); fflush(stderr);
}

void app::do_new_file()
{
    auto& w = M->world();
    w = world{};
    maybe_initialize_chunk_(chunk_coords{}, w[chunk_coords{}]);
}

} // namespace floormat
