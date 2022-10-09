#include "app.hpp"
#include <cstring>
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
    static thread_local auto t = progn(auto t = Magnum::Timeline{}; t.start(); return t; );

    [[maybe_unused]] volatile auto _type = type;
    [[maybe_unused]] volatile auto _id = id;
    [[maybe_unused]] volatile auto _src = src;
    [[maybe_unused]] volatile auto _severity = severity;
    [[maybe_unused]] volatile const char* _str = str.data();

    const Containers::ArrayView<const char> prefix{"Buffer detailed info: "};
    if (!std::strncmp(str.data(), prefix.data(), prefix.size()-1))
        str = str.exceptPrefix(prefix.size()-1);

    printf("%12.2f ", (double)t.previousFrameTime() * 1000);

    switch (severity)
    {
    using enum GL::DebugOutput::Severity;
    case Notification: std::fputs("DEBUG ", stdout); break;
    case Low: std::fputs("INFO  ", stdout); break;
    case Medium: std::fputs("NOTICE ", stdout); break;
    case High: std::fputs("ERROR ", stdout); break;
    default: std::fputs("????? ", stdout); break;
    }

    std::puts(str.data());
    std::fflush(stdout);
    std::fputs("", stdout); // put breakpoint here

    if (severity != Severity::Notification)
        std::abort();

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

    /* Disable rather spammy "Buffer detailed info" debug messages on NVidia drivers */
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other, {131185}, false);

    return nullptr;
}

} // namespace Magnum::Examples
