#pragma once
#include "src/pass-mode.hpp"
#include <memory>
#include <vector>
#include <Corrade/Containers/StringView.h>

#if defined _WIN32 && !defined FILENAME_MAX
#define FILENAME_MAX 260
#endif

namespace Magnum { using Vector2ub = Math::Vector2<unsigned char>; }
namespace floormat { struct serialized_scenery; }
namespace Magnum::Trade {
template<uint32_t> class ImageData;
using ImageData2D = ImageData<2>;
} // namespace Magnum::Trade

namespace floormat {

class tile_atlas;
class anim_atlas;
class wall_atlas;
struct scenery_proto;
struct vobj_info;
struct wall_info;

struct loader_
{
    virtual StringView shader(StringView filename) noexcept = 0;
    virtual Trade::ImageData2D texture(StringView prefix, StringView filename, bool fail_ok = true) noexcept(false) = 0;
    // todo remove Optional when wall_atlas is fully implemented -sh 20231122
    virtual std::shared_ptr<class tile_atlas> tile_atlas(StringView filename, Vector2ub size, Optional<pass_mode> pass) noexcept(false) = 0;
    virtual std::shared_ptr<class tile_atlas> tile_atlas(StringView filename) noexcept(false) = 0;
    virtual ArrayView<const String> anim_atlas_list() = 0;
    virtual std::shared_ptr<class anim_atlas> anim_atlas(StringView name, StringView dir = ANIM_PATH) noexcept(false) = 0;
    virtual const wall_info& wall_atlas(StringView name, bool fail_ok = true) = 0;
    virtual ArrayView<const wall_info> wall_atlas_list() = 0;
    static void destroy();
    static loader_& default_loader() noexcept;
    // todo move to ArrayView later, make non-static, and remove pass_mode
    static std::vector<std::shared_ptr<class tile_atlas>> tile_atlases(StringView filename, pass_mode p);
    virtual const std::vector<serialized_scenery>& sceneries() = 0;
    virtual const scenery_proto& scenery(StringView name) noexcept(false) = 0;
    virtual StringView startup_directory() noexcept = 0;
    static StringView strip_prefix(StringView name);
    virtual const vobj_info& vobj(StringView name) = 0;
    virtual ArrayView<const struct vobj_info> vobj_list() = 0;
    static StringView make_atlas_path(char(&buf)[FILENAME_MAX], StringView dir, StringView name);
    [[nodiscard]] static bool check_atlas_name(StringView name) noexcept;

    loader_(const loader_&) = delete;
    loader_& operator=(const loader_&) = delete;

    virtual ~loader_() noexcept;

    static const StringView IMAGE_PATH;
    static const StringView ANIM_PATH;
    static const StringView SCENERY_PATH;
    static const StringView TEMP_PATH;
    static const StringView VOBJ_PATH;
    static const StringView WALL_TILESET_PATH;

protected:
    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace floormat
