#include "loader.hpp"
#include "tile-atlas.hpp"
#include "compat/assert.hpp"
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/PluginManager.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/ImageView.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <unordered_map>
#include <utility>
#include <optional>

namespace Magnum::Examples {

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

    std::string shader(const Containers::StringView& filename) override;
    Trade::ImageData2D tile_texture(const Containers::StringView& filename) override;
    std::shared_ptr<struct tile_atlas> tile_atlas(const Containers::StringView& filename, Vector2i size) override;

    explicit loader_impl();
    ~loader_impl() override;
};

std::string loader_impl::shader(const Containers::StringView& filename)
{
    if (!shader_res)
        shader_res = std::make_optional<Utility::Resource>("game/shaders");
    auto ret = shader_res->getString(filename);
    if (ret.isEmpty())
        ABORT("can't find shader resource '%s'", filename.cbegin());
    return ret;
}

std::shared_ptr<tile_atlas> loader_impl::tile_atlas(const Containers::StringView& name, Vector2i size)
{
    auto it = atlas_map.find(name);
    if (it != atlas_map.end())
        return it->second;
    auto image = tile_texture(name);
    auto atlas = std::make_shared<struct tile_atlas>(image, size);
    atlas_map[name] = atlas;
    return atlas;
}

Trade::ImageData2D loader_impl::tile_texture(const Containers::StringView& filename)
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

loader_impl::loader_impl() = default;
loader_impl::~loader_impl() = default;

static loader_& make_default_loader()
{
    static loader_impl loader_singleton{};
    return loader_singleton;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
loader_& loader = make_default_loader();

} // namespace Magnum::Examples
