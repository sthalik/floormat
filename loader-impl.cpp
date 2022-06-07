#include "defs.hpp"
#include "loader.hpp"
#include "atlas.hpp"
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/PluginManager.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <unordered_map>
#include <utility>

namespace Magnum::Examples {

using atlas_ptr = std::shared_ptr<atlas_texture>;

struct loader_impl final : loader_
{
    const Utility::Resource shader_res{"game/shaders"};
    PluginManager::Manager<Trade::AbstractImporter> plugins;
    Containers::Pointer<Trade::AbstractImporter> tga_importer =
        plugins.loadAndInstantiate("TgaImporter");

    std::unordered_map<std::string, atlas_ptr> atlas_map;

    std::string shader(const Containers::StringView& filename) override;
    Trade::ImageData2D tile_texture(const Containers::StringView& filename) override;
    atlas_ptr tile_atlas(const Containers::StringView& filename, Vector2i size) override;

    explicit loader_impl();
    ~loader_impl() override;
};

std::string loader_impl::shader(const Containers::StringView& filename)
{
    auto ret = shader_res.getString(filename);
    if (ret.isEmpty())
        ABORT("can't find shader resource '%s'", filename.cbegin());
    return ret;
}

atlas_ptr loader_impl::tile_atlas(const Containers::StringView& name, Vector2i size)
{
    auto it = atlas_map.find(name);
    if (it != atlas_map.end())
        return it->second;
    auto atlas = std::make_shared<atlas_texture>(tile_texture(name), size);
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
    static loader_impl loader{};
    return loader;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
loader_& loader = make_default_loader();

} // namespace Magnum::Examples
