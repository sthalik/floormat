#pragma once

#include <Corrade/Containers/StringView.h>
#include <Magnum/Trade/ImageData.h>

#include <string>
#include <optional>
#include <memory>

#define IMAGE_PATH "share/floormat/images/"

namespace floormat {

struct tile_atlas;

struct loader_
{
    virtual std::string shader(Containers::StringView filename) = 0;
    virtual Trade::ImageData2D tile_texture(Containers::StringView filename) = 0;
    virtual std::shared_ptr<struct tile_atlas> tile_atlas(Containers::StringView filename, Vector2ub size) = 0;
    static void destroy();

    loader_(const loader_&) = delete;
    loader_& operator=(const loader_&) = delete;

    virtual ~loader_();

protected:
    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace floormat
