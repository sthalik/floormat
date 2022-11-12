#include "impl.hpp"
#include "src/loader.hpp"
#include "src/tile-atlas.hpp"
#include "compat/assert.hpp"
#include "compat/alloca.hpp"
#include "src/anim-atlas.hpp"
#include "src/emplacer.hpp"
#include <cstring>
#include <unordered_map>
#include <utility>
#include <memory>
#include <optional>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StringView.h>
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

namespace floormat {

struct loader_impl final : loader_
{
    std::optional<Utility::Resource> shader_res;
    PluginManager::Manager<Trade::AbstractImporter> importer_plugins;
    Containers::Pointer<Trade::AbstractImporter> image_importer =
        importer_plugins.loadAndInstantiate("StbImageImporter");

    Containers::Pointer<Trade::AbstractImporter> tga_importer =
        importer_plugins.loadAndInstantiate("TgaImporter");

    std::unordered_map<String, std::shared_ptr<struct tile_atlas>> tile_atlas_map;
    std::unordered_map<String, std::shared_ptr<struct anim_atlas>> anim_atlas_map;
    std::vector<String> anim_atlases;

    StringView shader(StringView filename) override;
    template<std::size_t N> Trade::ImageData2D texture(const char(&prefix)[N], StringView filename);
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
    const emplacer e{[&] { return std::make_shared<struct tile_atlas>(name, texture(FM_IMAGE_PATH, name), size); }};
    auto atlas = tile_atlas_map.try_emplace(name, e).first->second;
    return atlas;
}

template<std::size_t N>
fm_noinline
Trade::ImageData2D loader_impl::texture(const char(&prefix)[N], StringView filename_)
{
    if constexpr(N > 1)
        fm_assert(prefix[N-2] == '/');
    fm_assert(filename_.size() < 4096);
    fm_assert(filename_.find('\\') == filename_.end());
    fm_assert(filename_.find('\0') == filename_.end());
    fm_assert(tga_importer);
    constexpr std::size_t max_extension_length = 16;

    char* const filename = (char*)alloca(filename_.size() + N + max_extension_length);
    const std::size_t len = fm_begin(
        std::size_t off = N-1;
        if constexpr(N > 1)
            std::memcpy(filename, prefix, off);
        std::memcpy(filename + off, filename_.cbegin(), filename_.size());
        return off + filename_.size();
    );

    for (const auto& extension : std::initializer_list<StringView>{ ".tga", ".png", ".webp", })
    {
        std::memcpy(filename + len, extension.data(), extension.size());
        filename[len + extension.size()] = '\0';
        auto& importer = extension == StringView(".tga") ? tga_importer : image_importer;
        if (Path::exists(filename) && importer->openFile(filename))
        {
            auto img = importer->image2D(0);
            if (!img)
                fm_abort("can't allocate image for '%s'", filename);
            auto ret = std::move(*img);
            return ret;
        }
    }
    const auto path = Path::currentDirectory();
    filename[len] = '\0';
    fm_abort("can't open image '%s' (cwd '%s')", filename, path ? path->data() : "(null)");
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
        const auto path = Path::join(FM_ANIM_PATH, Path::splitExtension(name).first());
        auto anim_info = loader_detail::deserialize_anim(path + ".json");
        auto tex = texture("", path);

        fm_assert(!anim_info.anim_name.isEmpty() && !anim_info.object_name.isEmpty());
        fm_assert(anim_info.pixel_size.product() > 0);
        fm_assert(!anim_info.groups.empty());
        fm_assert(anim_info.nframes > 0);
        fm_assert(anim_info.nframes == 1 || anim_info.fps > 0);

        auto atlas = std::make_shared<struct anim_atlas>(path, tex, std::move(anim_info));
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
    if (const auto loc = Path::executableLocation())
    {
        auto path = *loc;
        if (const auto pos = path.findLast('/'); pos)
        {
            const auto size = std::size_t(pos.data() - path.data());
            path = path.prefix(size);
            loader_detail::chdir(Path::join(path, ".."));
            return;
        }
    }
    fm_warn("can't find install prefix!");
}

loader_impl::loader_impl()
{
    loader_detail::system_init();
    set_application_working_directory();
}

loader_impl::~loader_impl() = default;

loader_& loader_::default_loader() noexcept
{
    static loader_impl loader_singleton{};
    return loader_singleton;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
loader_& loader = loader_::default_loader();

} // namespace floormat
