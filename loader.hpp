#pragma once

#include <Corrade/Containers/StringView.h>
#include <Magnum/Trade/ImageData.h>

#include <string>
#include <optional>
#include <memory>

namespace Magnum::Examples {

struct texture_atlas;

struct loader_
{
    virtual std::string shader(const Containers::StringView& filename) = 0;
    virtual Trade::ImageData2D tile_texture(const Containers::StringView& filename) = 0;
    virtual std::shared_ptr<texture_atlas> tile_atlas(const Containers::StringView& filename, Vector2i size) = 0;
    static void destroy();

    loader_(const loader_&) = delete;
    loader_& operator=(const loader_&) = delete;

    virtual ~loader_();

protected:
    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace Magnum::Examples
