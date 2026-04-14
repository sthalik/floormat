#include "impl.hpp"
#include "compat/assert.hpp"
#include "ground-traits.hpp"
#include "ground-cell.hpp"
#include "wall-traits.hpp"
#include "wall-cell.hpp"
#include "anim-traits.hpp"
#include "anim-cell.hpp"
#include "scenery-traits.hpp"
#include "scenery-cell.hpp"
#include "vobj-cell.hpp"
#include "atlas-loader.hpp"
#include "atlas-loader-storage.hpp"
#include "serialize/json-wrapper.hpp"

namespace floormat {

} // namespace floormat

namespace floormat::loader_detail {

namespace {

constexpr auto default_font_filename = "imgui-default-ttf-font"_s;

} // namespace

StringView loader_impl::shader(StringView filename) noexcept
{
    if (!shader_res)
        shader_res = Optional<Utility::Resource>(InPlaceInit, "floormat/shaders");
    auto ret = shader_res->getString(filename);
    if (ret.isEmpty())
        fm_abort("can't find shader resource '%s'", filename.cbegin());
    return ret;
}

ArrayView<const void> loader_impl::font() noexcept
{
    if (!font_res)
        font_res = Optional<Utility::Resource>(InPlaceInit, "fonts");
    auto ret = font_res->getRaw("default-imgui-font");
    if (ret.isEmpty())
        fm_abort("can't find font resource '%s'", default_font_filename.data());
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

void loader_impl::destroy()
{
    _ground_loader = {InPlace};
    _wall_loader = {InPlace};
    _anim_loader = {InPlace};
    _scenery_loader = {InPlace};
    vobj_atlas_map.clear();
    vobjs.clear();
    // Free the GL texture while the context is still alive. The static
    // loader_impl's destructor fires at program exit — long after the GL
    // context is gone — so doing this in ~sprite_atlas() would be too late.
    // Also drop registry entries, since they point into the (now-empty)
    // atlas and would dangle if we kept them around.
    _sprite_registry.clear();
    _sprite_atlas.reset();
}


} // namespace floormat::loader_detail
