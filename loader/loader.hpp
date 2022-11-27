#pragma once
#include <memory>

namespace Magnum { using Vector2ub = Math::Vector2<unsigned char>; }
namespace floormat::Serialize { struct serialized_scenery; }

namespace floormat {

struct tile_atlas;
struct anim_atlas;
struct scenery_proto;

struct loader_
{
    virtual StringView shader(StringView filename) = 0;
    virtual std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename, Vector2ub size) = 0;
    virtual ArrayView<String> anim_atlas_list() = 0;
    virtual std::shared_ptr<struct anim_atlas> anim_atlas(StringView name) = 0;
    static void destroy();
    static loader_& default_loader() noexcept;
    static std::vector<std::shared_ptr<struct tile_atlas>> tile_atlases(StringView filename);
    static std::vector<Serialize::serialized_scenery> sceneries();

    loader_(const loader_&) = delete;
    loader_& operator=(const loader_&) = delete;

    virtual ~loader_();

    static const StringView IMAGE_PATH;
    static const StringView ANIM_PATH;

protected:
    loader_();
};

extern loader_& loader; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace floormat
