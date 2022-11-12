#include "impl.hpp"
#include "compat/assert.hpp"
#include "compat/alloca.hpp"
#include <cstring>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

fm_noinline
Trade::ImageData2D loader_impl::texture(StringView prefix, StringView filename_)
{
    ensure_plugins();

    const auto N = prefix.size();
    if (N > 0)
        fm_assert(prefix[N-1] == '/');
    fm_assert(filename_.size() < 4096);
    fm_assert(filename_.find('\\') == filename_.end());
    fm_assert(filename_.find('\0') == filename_.end());
    fm_assert(tga_importer);
    constexpr std::size_t max_extension_length = 16;

    char* const filename = (char*)alloca(filename_.size() + N + 1 + max_extension_length);
    if (N > 0)
        std::memcpy(filename, prefix.data(), N);
    std::memcpy(filename + N, filename_.cbegin(), filename_.size());
    std::size_t len =  filename_.size() + N;

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

} // namespace floormat::loader_detail
