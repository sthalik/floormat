#include "impl.hpp"

namespace floormat {

using loader_detail::loader_impl;

void loader_::destroy()
{
    loader.~loader_();
    new (&loader) loader_impl();
}

loader_& loader_::default_loader() noexcept
{
    static loader_impl loader_singleton{};
    return loader_singleton;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
loader_& loader = loader_::default_loader();

loader_::loader_() = default;
loader_::~loader_() noexcept = default;

StringView loader_::strip_prefix(StringView name)
{
    if (name.hasPrefix(IMAGE_PATH_))
        return name.exceptPrefix(IMAGE_PATH_.size());
    if (name.hasPrefix(ANIM_PATH))
        return name.exceptPrefix(ANIM_PATH.size());
    if (name.hasPrefix(SCENERY_PATH))
        return name.exceptPrefix(SCENERY_PATH.size());
    if (name.hasPrefix(VOBJ_PATH))
        return name.exceptPrefix(VOBJ_PATH.size());
    if (name.hasPrefix(GROUND_TILESET_PATH))
        return name.exceptPrefix(GROUND_TILESET_PATH.size());
    if (name.hasPrefix(WALL_TILESET_PATH))
        return name.exceptPrefix(WALL_TILESET_PATH.size());
    return name;
}

const StringView loader_::INVALID = "<invalid>"_s; // todo use it
const StringView loader_::IMAGE_PATH_ = "images/"_s;
const StringView loader_::ANIM_PATH = "anim/"_s;
const StringView loader_::SCENERY_PATH = "scenery/"_s;
const StringView loader_::TEMP_PATH = "../../../"_s;
const StringView loader_::VOBJ_PATH = "vobj/"_s;
const StringView loader_::GROUND_TILESET_PATH = "ground/"_s;
const StringView loader_::WALL_TILESET_PATH = "walls/"_s;

} // namespace floormat
