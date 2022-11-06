#pragma once

#include <memory>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StringView.h>
#include <Magnum/Trade/ImageData.h>

#define FM_IMAGE_PATH "share/floormat/images/"
#define FM_ANIM_PATH "share/floormat/anim/"

namespace floormat {

struct tile_atlas;
struct anim_atlas;

struct loader_
{
    virtual StringView shader(StringView filename) = 0;
    virtual Trade::ImageData2D tile_texture(StringView filename) = 0;
    virtual std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename, Vector2ub size) = 0;
    virtual ArrayView<String> anim_atlas_list() = 0;
    virtual std::shared_ptr<struct anim_atlas> anim_atlas(StringView name) = 0;
    static void destroy();

    loader_(const loader_&) = delete;
    loader_& operator=(const loader_&) = delete;

    virtual ~loader_();

protected:
    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace floormat
