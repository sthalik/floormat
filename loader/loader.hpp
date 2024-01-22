#pragma once
#include "compat/defs.hpp"
#include "src/pass-mode.hpp"
#include <stdio.h>
#include <memory>
#include <Corrade/Containers/String.h>

namespace Magnum { using Vector2ub = Math::Vector2<unsigned char>; }
namespace floormat { struct serialized_scenery; }
namespace Magnum::Trade {
template<uint32_t> class ImageData;
using ImageData2D = ImageData<2>;
} // namespace Magnum::Trade

namespace floormat {

class anim_atlas;
struct scenery_proto;
struct vobj_info;
class ground_atlas;
struct ground_info;
struct wall_info;
class wall_atlas;

struct vobj_info final
{
    String name, descr;
    std::shared_ptr<anim_atlas> atlas;
};

enum class loader_policy : uint8_t
{
    error, warn, ignore, DEFAULT = error,
};

struct loader_
{
    virtual StringView shader(StringView filename) noexcept = 0;
    virtual Trade::ImageData2D texture(StringView prefix, StringView filename) noexcept(false) = 0;
    virtual std::shared_ptr<class ground_atlas> get_ground_atlas(StringView name, Vector2ub size, pass_mode pass) noexcept(false) = 0;
    virtual std::shared_ptr<class ground_atlas> ground_atlas(StringView filename, loader_policy policy = loader_policy::DEFAULT) noexcept(false) = 0;
    virtual ArrayView<const String> anim_atlas_list() = 0;
    virtual std::shared_ptr<class anim_atlas> anim_atlas(StringView name, StringView dir = ANIM_PATH) noexcept(false) = 0;
    virtual std::shared_ptr<class wall_atlas> wall_atlas(StringView name, loader_policy policy = loader_policy::DEFAULT) noexcept(false) = 0;
    virtual ArrayView<const wall_info> wall_atlas_list() = 0;
    virtual void destroy() = 0;
    static loader_& default_loader() noexcept;
    virtual ArrayView<const ground_info> ground_atlas_list() noexcept(false) = 0;
    virtual ArrayView<const serialized_scenery> sceneries() = 0;
    virtual const scenery_proto& scenery(StringView name) noexcept(false) = 0;
    virtual StringView startup_directory() noexcept = 0;
    static StringView strip_prefix(StringView name);
    virtual const vobj_info& vobj(StringView name) = 0;
    virtual ArrayView<const struct vobj_info> vobj_list() = 0;
    static StringView make_atlas_path(char(&buf)[FILENAME_MAX], StringView dir, StringView name);
    [[nodiscard]] static bool check_atlas_name(StringView name) noexcept;

    virtual ~loader_() noexcept;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(loader_);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(loader_);

    static const StringView INVALID;
    static const StringView IMAGE_PATH_;
    static const StringView ANIM_PATH;
    static const StringView SCENERY_PATH;
    static const StringView TEMP_PATH;
    static const StringView VOBJ_PATH;
    static const StringView GROUND_TILESET_PATH;
    static const StringView WALL_TILESET_PATH;

protected:
    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace floormat
