#include "impl.hpp"
#include "compat/assert.hpp"
#include "loader/scenery.hpp"
#include "loader/vobj-info.hpp"
#include <Corrade/Containers/Pair.h>
#include <Magnum/Trade/ImageData.h>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Walloca"
#endif

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

loader_impl::loader_impl()
{
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

} // namespace floormat::loader_detail
