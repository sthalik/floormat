#include "impl.hpp"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include "compat/exception.hpp"
#include "compat/strerror.hpp"
#include <cstring>
//#include <cstdio>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

fm_noinline
Trade::ImageData2D loader_impl::texture(StringView prefix, StringView filename_) noexcept(false)
{
    ensure_plugins();

    constexpr size_t max_extension_length = 16;
    const auto N = prefix.size();
    if (N > 0) [[likely]]
        fm_assert(prefix[N-1] == '/');
    fm_soft_assert(filename_.size() + prefix.size() + max_extension_length + 1 < fm_FILENAME_MAX);
    fm_soft_assert(check_atlas_name(filename_));
    fm_soft_assert(tga_importer);

    char buf[fm_FILENAME_MAX];
    const auto path_no_ext = make_atlas_path(buf, prefix, filename_);
    const auto len = path_no_ext.size();

    for (auto extension : { ".tga"_s, ".png"_s, ".webp"_s, })
    {
        fm_soft_assert(len + extension.size() < std::size(buf));
        std::memcpy(buf + len, extension.data(), extension.size());
        buf[len + extension.size()] = '\0';
        auto path = StringView{buf, len + extension.size(), StringViewFlag::NullTerminated};
        fm_debug_assert(path.size() < std::size(buf));
        auto& importer = extension == ".tga"_s ? tga_importer : image_importer;
        if (Path::exists(path) && importer->openFile(path))
        {
            auto img = importer->image2D(0);
            if (!img)
                fm_abort("can't allocate image for '%s'", buf);
            auto ret = move(*img);
            return ret;
        }
    }

    const auto path = Path::currentDirectory();
    buf[len] = '\0';
    char errbuf[128];
    fm_throw("can't open image '{}' (cwd '{}'): {}"_cf, buf, path ? StringView{*path} : "(null)"_s, get_error_string(errbuf));
}

} // namespace floormat::loader_detail
