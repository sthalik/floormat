#include "impl.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "compat/defs.hpp"
#include <cstring>
#include <cstdio>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

fm_noinline
Trade::ImageData2D loader_impl::texture(StringView prefix, StringView filename_) noexcept(false)
{
    ensure_plugins();

    constexpr size_t max_extension_length = 16;
    const auto N = prefix.size();
    if (N > 0)
        fm_assert(prefix[N-1] == '/');
    fm_soft_assert(filename_.size() + prefix.size() + max_extension_length + 1 < FILENAME_MAX);
    fm_soft_assert(check_atlas_name(filename_));
    fm_soft_assert(tga_importer);

    char filename[FILENAME_MAX];
    if (N > 0)
        std::memcpy(filename, prefix.data(), N);
    std::memcpy(filename + N, filename_.data(), filename_.size());
    size_t len =  filename_.size() + N;

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
    fm_throw("can't open image '{}' (cwd '{}')"_cf, filename, path ? StringView{*path} : "(null)"_s);
}

} // namespace floormat::loader_detail
