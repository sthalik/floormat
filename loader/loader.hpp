#pragma once
#include "src/pass-mode.hpp"
#include <memory>
#include <vector>
#include <Corrade/Containers/StringView.h>

namespace Magnum { using Vector2ub = Math::Vector2<unsigned char>; }
namespace floormat { struct serialized_scenery; }

namespace floormat {

struct tile_atlas;
struct anim_atlas;
struct scenery_proto;

struct loader_
{
    virtual StringView shader(StringView filename) noexcept = 0;
    virtual std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename, Vector2ub size, Optional<pass_mode> pass) noexcept(false) = 0;
    virtual std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename) noexcept(false) = 0;
    virtual ArrayView<String> anim_atlas_list() = 0;
    virtual std::shared_ptr<struct anim_atlas> anim_atlas(StringView name, StringView dir = ANIM_PATH) noexcept(false) = 0;
    static void destroy();
    static loader_& default_loader() noexcept;
    static std::vector<std::shared_ptr<struct tile_atlas>> tile_atlases(StringView filename, pass_mode p);
    virtual const std::vector<serialized_scenery>& sceneries() = 0;
    virtual const scenery_proto& scenery(StringView name) noexcept(false) = 0;
    virtual StringView startup_directory() noexcept = 0;

    loader_(const loader_&) = delete;
    loader_& operator=(const loader_&) = delete;

    virtual ~loader_();

    static const StringView IMAGE_PATH;
    static const StringView ANIM_PATH;
    static const StringView SCENERY_PATH;

protected:
    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace floormat
