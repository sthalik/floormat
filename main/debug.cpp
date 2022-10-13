#include "app.hpp"
#include <chrono>
#include <Magnum/GL/Renderer.h>

namespace floormat {

using Feature = GL::Renderer::Feature;
using Source = GL::DebugOutput::Source;
using Type = GL::DebugOutput::Type;
using Severity = GL::DebugOutput::Severity;
using GL::Renderer;
using GL::DebugOutput;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void app::debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                         Severity severity, const std::string& str) const
{
    static thread_local auto clock = std::chrono::steady_clock{};
    static const auto t0 = clock.now();

#if 0
    [[maybe_unused]] volatile auto _type = type;
    [[maybe_unused]] volatile auto _id = id;
    [[maybe_unused]] volatile auto _src = src;
    [[maybe_unused]] volatile auto _severity = severity;
    [[maybe_unused]] volatile const char* _str = str.data();
#endif
    (void)src; (void)type;

    const char* p = str.c_str();
    if (str.starts_with("Buffer detailed info: "))
        p = str.data() + sizeof("Buffer detailed info: ") - 1;

    using seconds = std::chrono::duration<double>;
    const auto t = std::chrono::duration_cast<seconds>(clock.now() - t0).count();
    printf("[%10.03f] ", t);

    switch (severity)
    {
    using enum GL::DebugOutput::Severity;
    case Notification: std::fputs("DEBUG ", stdout); break;
    case Low: std::fputs("INFO  ", stdout); break;
    case Medium: std::fputs("NOTICE ", stdout); break;
    case High: std::fputs("ERROR ", stdout); break;
    default: std::fputs("????? ", stdout); break;
    }
    printf("%6u ", id);

    std::puts(p);
    std::fflush(stdout);
    std::fputs("", stdout); // put breakpoint here

#if 0
    if (severity != Severity::Notification)
        std::abort();
#endif

    std::fputs("", stdout); // put breakpoint here
}

static void _debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                            GL::DebugOutput::Severity severity, const std::string& str, const void* self)
{
    static_cast<const app*>(self)->debug_callback(src, type, id, severity, str);
}

void* app::register_debug_callback()
{
    GL::Renderer::setFeature(Feature::DebugOutput, true);
    GL::Renderer::setFeature(Feature::DebugOutputSynchronous, true);
    GL::DebugOutput::setCallback(_debug_callback, this);
    GL::DebugOutput::setEnabled(true);

#if 1
    /* Disable rather spammy "Buffer detailed info" debug messages on NVidia drivers */
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other, {131185}, false);
#endif

    return nullptr;
}

} // namespace floormat
