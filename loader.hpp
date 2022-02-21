#pragma once

#include <Magnum/Trade/ImageData.h>

#include <string>
#include <optional>
#include <memory>

namespace Magnum::Examples {

struct atlas_texture;

struct loader_
{
    virtual std::string shader(const std::string& filename) = 0;
    virtual Trade::ImageData2D tile_texture(const std::string& filename) = 0;
    virtual std::shared_ptr<atlas_texture> tile_atlas(const std::string& filename) = 0;
    static void destroy();

    loader_(const loader_&) = delete;
    loader_& operator=(const loader_&) = delete;

    virtual ~loader_();

protected:
    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace Magnum::Examples
