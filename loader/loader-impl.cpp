#include "src/loader.hpp"
#include "src/tile-atlas.hpp"
#include "compat/assert.hpp"
#include "compat/alloca.hpp"
#include "src/anim-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <optional>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Containers/StringStlHash.h>
#include <Corrade/PluginManager/PluginManager.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImageConverter.h>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Walloca"
#endif

using StringView = Corrade::Containers::StringView;

namespace Path = Corrade::Utility::Path;

namespace floormat {

struct loader_impl final : loader_
{
    std::optional<Utility::Resource> shader_res;
    PluginManager::Manager<Trade::AbstractImporter> importer_plugins;
    Containers::Pointer<Trade::AbstractImporter> tga_importer =
        importer_plugins.loadAndInstantiate("AnyImageImporter");

    PluginManager::Manager<Trade::AbstractImageConverter> image_converter_plugins;

    std::unordered_map<std::string, std::shared_ptr<struct tile_atlas>> tile_atlas_map;
    std::unordered_map<StringView, std::shared_ptr<struct anim_atlas>> anim_atlas_map;
    std::vector<String> anim_atlases;

    StringView shader(StringView filename) override;
    Trade::ImageData2D tile_texture(StringView filename) override;
    std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename, Vector2ub size) override;
    ArrayView<String> anim_atlas_list() override;
    std::shared_ptr<struct anim_atlas> anim_atlas(StringView name) override;

    void get_anim_atlas_list();

    static void set_application_working_directory();

    explicit loader_impl();
    ~loader_impl() override;
};

StringView loader_impl::shader(StringView filename)
{
    if (!shader_res)
        shader_res = std::make_optional<Utility::Resource>("floormat/shaders");
    auto ret = shader_res->getString(filename);
    if (ret.isEmpty())
        fm_abort("can't find shader resource '%s'", filename.cbegin());
    return ret;
}

std::shared_ptr<tile_atlas> loader_impl::tile_atlas(StringView name, Vector2ub size)
{
    auto it = std::find_if(tile_atlas_map.cbegin(), tile_atlas_map.cend(), [&](const auto& x) {
        const auto& [k, v] = x;
        return StringView{k} == name;
    });
    if (it != tile_atlas_map.cend())
        return it->second;
    auto image = tile_texture(name);
    auto atlas = std::make_shared<struct tile_atlas>(name, image, size);
    tile_atlas_map[name] = atlas;
    return atlas;
}

Trade::ImageData2D loader_impl::tile_texture(StringView filename_)
{
    static_assert(FM_IMAGE_PATH[sizeof(FM_IMAGE_PATH)-2] == '/');
    fm_assert(filename_.size() < 4096);
    fm_assert(filename_.find('\\') == filename_.end());
    fm_assert(filename_.find('\0') == filename_.end());
    fm_assert(tga_importer);
    constexpr std::size_t max_extension_length = 16;

    char* const filename = (char*)alloca(filename_.size() + std::size(FM_IMAGE_PATH) + max_extension_length);
    const std::size_t len = fm_begin(
        std::size_t off = std::size(FM_IMAGE_PATH)-1;
        std::memcpy(filename, FM_IMAGE_PATH, off);
        std::memcpy(filename + off, filename_.cbegin(), filename_.size());
        return off + filename_.size();
    );

    for (const auto& extension : std::initializer_list<StringView>{ ".tga", ".png", ".webp", })
    {
        std::memcpy(filename + len, extension.data(), extension.size());
        filename[len + extension.size()] = '\0';
        if (Path::exists(filename) && tga_importer->openFile(filename))
        {
            auto img = tga_importer->image2D(0);
            if (!img)
                fm_abort("can't allocate tile image for '%s'", filename);
            auto ret = std::move(*img);
            return ret;
        }
        else
        {
            fm_warn("can't open '%s'", filename);
        }
    }
    const auto path = Path::currentDirectory();
    filename[len] = '\0';
    fm_abort("can't open tile image '%s' (cwd '%s')", filename, path ? path->data() : "(null)");
}

ArrayView<String> loader_impl::anim_atlas_list()
{
    if (anim_atlases.empty())
        get_anim_atlas_list();
    return anim_atlases;
}

std::shared_ptr<anim_atlas> loader_impl::anim_atlas(StringView name)
{
    if (auto it = anim_atlas_map.find(name); it != anim_atlas_map.end())
        return it->second;
    else
    {
        const auto path = Path::join(FM_ANIM_PATH, name);
        std::filesystem::path p = std::string_view{path};
        p.replace_extension("json");
        auto anim_info = json_helper::from_json<Serialize::anim>(p);
        p.replace_extension({});
        auto tex = tile_texture(path);

        fm_assert(!anim_info.anim_name.isEmpty() && !anim_info.object_name.isEmpty());
        fm_assert(anim_info.pixel_size.product() > 0);
        fm_assert(!anim_info.groups.empty());
        fm_assert(anim_info.nframes > 0);
        fm_assert(anim_info.nframes == 1 || anim_info.fps > 0);

        auto atlas = std::make_shared<struct anim_atlas>(p.string(), tex, std::move(anim_info));
        return anim_atlas_map[atlas->name()] = atlas;
    }
}

void loader_impl::get_anim_atlas_list()
{
    anim_atlases.clear();
    anim_atlases.reserve(64);
    using f = Path::ListFlag;
    constexpr auto flags = f::SkipDirectories | f::SkipDotAndDotDot | f::SkipSpecial | f::SortAscending;
    if (const auto list = Path::list(FM_ANIM_PATH, flags); list)
        for (StringView str : *list)
            if (str.hasSuffix(".json"))
                anim_atlases.emplace_back(str.exceptSuffix(std::size(".json")-1));
}

void loader_::destroy()
{
    loader.~loader_();
    new (&loader) loader_impl();
}

void loader_impl::set_application_working_directory()
{
    static bool once = false;
    if (once)
        return;
    once = true;
    const auto location = Path::executableLocation();
    if (!location)
        return;
    std::filesystem::path path((std::string)*location);
    path.replace_filename("..");
    std::error_code error;
    std::filesystem::current_path(path, error);
    if (error.value()) {
        fm_warn("failed to change working directory to '%s' (%s)",
             path.string().data(), error.message().data());
    }
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

} // namespace floormat
