#pragma once
#include "compat/setenv.hpp"
#include <cr/StringView.h>
#include <mg/Extensions.h>
#include <mg/GLContext.h>

namespace floormat {

// zink with descriptor buffers loses pixel buffer transfers over NVIDIA's Vulkan driver.
// Call it before the first GL context, since zink reads ZINK_DESCRIPTORS when that context is made.
inline void disable_zink_descriptor_buffers()
{
    setenv("ZINK_DESCRIPTORS", "lazy", 0);
}

// Returns the extensions floormat disables on the driver of the current GL context.
// Call it while no GL::Context exists, since it makes one to read the renderer.
inline ArrayView<const GL::Extension> get_broken_gl_extensions()
{
    // zink binds glTexPageCommitmentARB() regions as if they started at the origin. Committing from the origin
    // works, but Mesa caps sparse storage at max_texture_mb, which on zink is too small for an atlas.
    static constexpr GL::Extension zink_extensions[] = { GL::Extensions::ARB::sparse_texture{}, };

    Platform::GLContext context{NoCreate};
    if (context.tryCreate(GL::Context::Configuration{}.addFlags(GL::Context::Configuration::Flag::QuietLog)) &&
        context.rendererString().hasPrefix("zink"_s))
        return zink_extensions;
    else
        return {};
}

} // namespace floormat
