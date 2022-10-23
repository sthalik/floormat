#include "floormat-main-impl.hpp"
#include <chrono>

namespace floormat {

using Severity = GL::DebugOutput::Severity;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void main_impl::debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                         Severity severity, const std::string& str) const
{
    static thread_local auto clock = std::chrono::steady_clock{};
    static const auto t0 = clock.now();

#if 1
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
    case Notification: std::fputs("fm_debug ", stdout); break;
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

void main_impl::_debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                          GL::DebugOutput::Severity severity, const std::string& str, const void* self)
{
    static_cast<const main_impl*>(self)->debug_callback(src, type, id, severity, str);
}

void main_impl::register_debug_callback() noexcept
{
    GL::DebugOutput::setCallback(_debug_callback, this);

#if 1
    /* Disable rather spammy "Buffer detailed info" debug messages on NVidia drivers */
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other, {131185}, false);
#endif
}

} // namespace floormat
