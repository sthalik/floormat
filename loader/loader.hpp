#pragma once
#include <memory>

namespace Corrade::Containers { template<typename T> class ArrayView; class String; }
namespace Corrade::Containers { template<typename T> class BasicStringView; using StringView = BasicStringView<const char>; }
namespace Magnum::Math { template<class T> class Vector2; }
namespace Magnum { using Vector2ub = Math::Vector2<unsigned char>; }

namespace floormat {

struct tile_atlas;
struct anim_atlas;

struct loader_
{
    virtual StringView shader(StringView filename) = 0;
    virtual std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename, Vector2ub size) = 0;
    virtual ArrayView<String> anim_atlas_list() = 0;
    virtual std::shared_ptr<struct anim_atlas> anim_atlas(StringView name) = 0;
    static void destroy();
    static loader_& default_loader() noexcept;

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
