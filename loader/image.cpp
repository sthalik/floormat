#include "impl.hpp"
#include "compat/exception.hpp"
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImporter.h>

namespace floormat::loader_detail {

Trade::ImageData2D loader_impl::image(StringView path) noexcept(false)
{
    ensure_plugins();
    if (!image_importer->openFile(path))
        fm_throw("can't open image '{}'"_cf, path);
    auto img = image_importer->image2D(0);
    if (!img)
        fm_throw("can't decode image '{}'"_cf, path);
    auto ret = move(*img);
    return ret;
}

} // namespace floormat::loader_detail
