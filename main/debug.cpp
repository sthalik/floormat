#include "app.hpp"
#include <Magnum/GL/Renderer.h>

namespace Magnum::Examples {

using Feature = GL::Renderer::Feature;
using Source = GL::DebugOutput::Source;
using Type = GL::DebugOutput::Type;
using Severity = GL::DebugOutput::Severity;
using GL::Renderer;
using GL::DebugOutput;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void app::debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                         Severity severity, Containers::StringView str) const
{
    [[maybe_unused]] volatile auto _type = type;
    [[maybe_unused]] volatile auto _id = id;
    [[maybe_unused]] volatile auto _src = src;
    [[maybe_unused]] volatile auto _severity = severity;
    [[maybe_unused]] volatile const char* _str = str.data();

    std::puts(str.data());
    std::fflush(stdout);
    std::fputs("", stdout); // put breakpoint here
}

static void _debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                            GL::DebugOutput::Severity severity, Containers::StringView str, const void* self)
{
    static_cast<const app*>(self)->debug_callback(src, type, id, severity, str);
}

void* app::register_debug_callback()
{
    GL::Renderer::setFeature(Feature::DebugOutput, true);
    GL::Renderer::setFeature(Feature::DebugOutputSynchronous, true);
    GL::DebugOutput::setCallback(_debug_callback, this);
    GL::DebugOutput::setEnabled(true);
    //GL::DebugOutput::setEnabled(Severity::Notification, false);

    return nullptr;
}

} // namespace Magnum::Examples
