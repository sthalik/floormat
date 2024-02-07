#include "impl.hpp"
#include "compat/assert.hpp"
#include "scenery.hpp"
#include "wall-cell.hpp"
#include "anim-cell.hpp"
#include "ground-cell.hpp"
#include "ground-traits.hpp"
#include "atlas-loader.hpp"
#include "atlas-loader-storage.hpp"

#include <Corrade/Containers/Pair.h>
#include <Magnum/Trade/ImageData.h>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Walloca"
#endif

namespace floormat {

} // namespace floormat

namespace floormat::loader_detail {

StringView loader_impl::shader(StringView filename) noexcept
{
    if (!shader_res)
        shader_res = Optional<Utility::Resource>(InPlaceInit, "floormat/shaders");
    auto ret = shader_res->getString(filename);
    if (ret.isEmpty())
        fm_abort("can't find shader resource '%s'", filename.cbegin());
    return ret;
}

loader_impl::loader_impl() :
     _ground_loader{ make_ground_atlas_loader() }
{
    missing_wall_atlases.reserve(32);
    system_init();
    set_application_working_directory();
}

loader_impl::~loader_impl() = default;

void loader_impl::ensure_plugins()
{
    if (importer_plugins)
        return;

    importer_plugins.emplace();
    image_importer = importer_plugins->loadAndInstantiate("StbImageImporter");
    tga_importer = importer_plugins->loadAndInstantiate("TgaImporter");

    fm_assert(image_importer);
    fm_assert(tga_importer);
}

void loader_impl::destroy()
{
    wall_atlas_map.clear();
    wall_atlas_array.clear();
    invalid_wall_atlas = nullptr;
    missing_wall_atlases.clear();
    _ground_loader = {InPlace};
    anim_atlas_map.clear();
    anim_atlases.clear();
    invalid_anim_atlas = nullptr;
    sceneries_map.clear();
    sceneries_array.clear();

    vobj_atlas_map.clear();
    vobjs.clear();
}


} // namespace floormat::loader_detail
