#pragma once
#include "compat/defs.hpp"
#include "src/pass-mode.hpp"
#include "loader/policy.hpp"
#include <memory>
#include <cr/StringView.h>

//namespace Magnum { using Vector2ub = Math::Vector2<unsigned char>; }
namespace Magnum::Trade { template<uint32_t> class ImageData; using ImageData2D = ImageData<2>; }

namespace floormat {

struct scenery_cell;
namespace loader_detail {}
namespace Serialize {}

struct anim_def;
class anim_atlas;
struct anim_cell;
struct scenery_proto;
struct vobj_cell;
class ground_atlas;
struct ground_cell;
struct wall_cell;
class wall_atlas;
struct scenery_proto;
struct json_wrapper;

struct loader_
{
    virtual void destroy() = 0;
    static loader_& default_loader() noexcept;
    virtual StringView shader(StringView filename) noexcept = 0;
    virtual Trade::ImageData2D make_error_texture(Vector2ui size) = 0;
    virtual Trade::ImageData2D make_error_texture(Vector2ui size, Vector4ub color) = 0;
    virtual Trade::ImageData2D texture(StringView prefix, StringView filename) noexcept(false) = 0;

    virtual const std::shared_ptr<class ground_atlas>& ground_atlas(StringView filename, loader_policy policy = loader_policy::DEFAULT) noexcept(false) = 0;
    virtual const std::shared_ptr<class wall_atlas>& wall_atlas(StringView name, loader_policy policy = loader_policy::DEFAULT) noexcept(false) = 0;
    virtual std::shared_ptr<class anim_atlas> anim_atlas(StringView name, StringView dir, loader_policy policy = loader_policy::DEFAULT) noexcept(false) = 0;
    virtual const struct scenery_proto& scenery(StringView name, loader_policy policy = loader_policy::DEFAULT) = 0;

    virtual ArrayView<const ground_cell> ground_atlas_list() noexcept(false) = 0;
    virtual ArrayView<const wall_cell> wall_atlas_list() = 0;
    virtual ArrayView<const anim_cell> anim_atlas_list() = 0;
    virtual ArrayView<const scenery_cell> scenery_list() = 0;

    virtual StringView startup_directory() noexcept = 0;
    static StringView strip_prefix(StringView name);
    virtual const vobj_cell& vobj(StringView name) = 0;
    virtual ArrayView<const struct vobj_cell> vobj_list() = 0;
    static StringView make_atlas_path(char(&buf)[fm_FILENAME_MAX], StringView dir, StringView name, StringView extension = {});
    [[nodiscard]] static bool check_atlas_name(StringView name) noexcept;

    virtual const wall_cell& invalid_wall_atlas() = 0;
    virtual const ground_cell& invalid_ground_atlas() = 0;
    virtual const anim_cell& invalid_anim_atlas() = 0;
    virtual const scenery_cell& invalid_scenery_atlas() = 0;

    /** \deprecated{internal use only}*/ [[nodiscard]]
    virtual std::shared_ptr<class ground_atlas> get_ground_atlas(StringView name, Vector2ub size, pass_mode pass) noexcept(false) = 0;
    /** \deprecated{internal use only}*/ [[nodiscard]]
    virtual std::shared_ptr<class wall_atlas> get_wall_atlas(StringView name) noexcept(false) = 0;
    /** \deprecated{internal use only}*/ [[nodiscard]]
    virtual std::shared_ptr<class anim_atlas> get_anim_atlas(StringView path) noexcept(false) = 0;
    /** \deprecated{internal use only}*/ [[nodiscard]]
    virtual struct scenery_proto get_scenery(StringView filename, const scenery_cell& c) noexcept(false) = 0;

    virtual ~loader_() noexcept;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(loader_);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(loader_);

    static constexpr StringView INVALID = "<invalid>"_s;
    static const StringView ANIM_PATH;
    static const StringView SCENERY_PATH;
    static const StringView TEMP_PATH;
    static const StringView VOBJ_PATH;
    static const StringView GROUND_TILESET_PATH;
    static const StringView WALL_TILESET_PATH;

protected:
    static anim_def deserialize_anim_def(StringView filename) noexcept(false);

    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace floormat
