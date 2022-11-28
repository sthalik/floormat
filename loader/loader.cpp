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
loader_::~loader_() = default;

const StringView loader_::IMAGE_PATH = "share/floormat/images/"_s;
const StringView loader_::ANIM_PATH = "share/floormat/anim/"_s;
const StringView loader_::SCENERY_PATH = "share/floormat/scenery/"_s;

} // namespace floormat
