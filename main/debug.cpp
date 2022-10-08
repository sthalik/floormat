#include "app.hpp"
#include <Magnum/GL/Renderer.h>

namespace Magnum::Examples {

using Feature = GL::Renderer::Feature;
using Severity = GL::DebugOutput::Severity;
using Type = GL::DebugOutput::Type;
using GL::Renderer;
using GL::DebugOutput;

void app::debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                         GL::DebugOutput::Severity severity, const std::string& str) const
{
    std::fputs(str.c_str(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
    std::puts(""); // put breakpoint here
}

static void _debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                            GL::DebugOutput::Severity severity, const std::string& str, const void* self)
{
    static_cast<const app*>(self)->debug_callback(src, type, id, severity, str);
}

void app::register_debug_callback()
{
    GL::Renderer::setFeature(Feature::DebugOutput, true);
    GL::Renderer::setFeature(Feature::DebugOutputSynchronous, true);
    GL::DebugOutput::setCallback(_debug_callback, this);
    GL::DebugOutput::setEnabled(Severity::High, true);
}

} // namespace Magnum::Examples
