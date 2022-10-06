#include "loader.hpp"
#include "tile-atlas.hpp"
#include "compat/assert.hpp"
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <optional>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/PluginManager/PluginManager.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImageConverter.h>

namespace Magnum::Examples {

static void set_application_working_directory()
{
    static bool once = false;
    if (once)
        return;
    once = true;
    const auto location = *Utility::Path::executableLocation();
    if (const auto dir = Utility::Path::split(location).first(); !dir.isEmpty()) {
        const std::filesystem::path path(std::string{dir});
        std::filesystem::current_path(path/"..");
    }
}

struct loader_impl final : loader_
{
    std::optional<Utility::Resource> shader_res;
    PluginManager::Manager<Trade::AbstractImporter> importer_plugins;
    Containers::Pointer<Trade::AbstractImporter> tga_importer =
        importer_plugins.loadAndInstantiate("AnyImageImporter");

    PluginManager::Manager<Trade::AbstractImageConverter> image_converter_plugins;
    Containers::Pointer<Trade::AbstractImageConverter> tga_converter =
        image_converter_plugins.loadAndInstantiate("AnyImageConverter");

    std::unordered_map<std::string, std::shared_ptr<struct tile_atlas>> atlas_map;

    std::string shader(Containers::StringView filename) override;
    Trade::ImageData2D tile_texture(Containers::StringView filename) override;
    std::shared_ptr<struct tile_atlas> tile_atlas(Containers::StringView filename, Vector2ui size) override;

    explicit loader_impl();
    ~loader_impl() override;
};

std::string loader_impl::shader(Containers::StringView filename)
{
    if (!shader_res)
        shader_res = std::make_optional<Utility::Resource>("game/shaders");
    auto ret = shader_res->getString(filename);
    if (ret.isEmpty())
        ABORT("can't find shader resource '%s'", filename.cbegin());
    return ret;
}

std::shared_ptr<tile_atlas> loader_impl::tile_atlas(Containers::StringView name, Vector2ui size)
{
    auto it = atlas_map.find(name);
    if (it != atlas_map.end())
        return it->second;
    auto image = tile_texture(name);
    auto atlas = std::make_shared<struct tile_atlas>(name, image, size);
    atlas_map[name] = atlas;
    return atlas;
}

Trade::ImageData2D loader_impl::tile_texture(Containers::StringView filename)
{
    if(!tga_importer || !tga_importer->openFile(filename))
        ABORT("can't open tile image '%s'", filename.cbegin());
    auto img = tga_importer->image2D(0);
    if (!img)
        ABORT("can't allocate tile image for '%s'", filename.cbegin());
    auto ret = std::move(*img);
    return ret;
}

void loader_::destroy()
{
    loader.~loader_();
    new (&loader) loader_impl();
}

loader_impl::loader_impl()
{
    set_application_working_directory();
}

loader_impl::~loader_impl() = default;

static loader_& make_default_loader()
{
    static loader_impl loader_singleton{};
    return loader_singleton;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
loader_& loader = make_default_loader();

} // namespace Magnum::Examples
