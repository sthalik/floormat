#include "serialize.hpp"

#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>

#include "json-magnum.hpp"

using Corrade::Utility::Error;

#if defined __clang__ || defined __CLION_IDE__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wweak-vtables"
#   pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_frame, ground, offset, size)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_group, name, frames, ground)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim, name, nframes, actionframe, fps, groups, width, height)

#if defined __clang__ || defined __CLION_IDE__
#   pragma clang diagnostic pop
#endif

std::tuple<anim, bool> anim::from_json(const std::filesystem::path& pathname) noexcept
{
    return json_helper<anim>::from_json(pathname);
}

bool anim::to_json(const std::filesystem::path& pathname) const noexcept
{
    return json_helper<anim>::to_json(*this, pathname);
}
