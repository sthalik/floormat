#include "main-impl.hpp"
#include <chrono>

namespace floormat {

using Severity = GL::DebugOutput::Severity;

CORRADE_NEVER_INLINE void gl_debug_put_breakpoint_here();
void gl_debug_put_breakpoint_here()  {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void main_impl::debug_callback(unsigned src, unsigned type, unsigned id, unsigned severity, StringView str) const
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

    if (auto pfx = "Buffer detailed info: "_s; str.hasPrefix(pfx))
        str = str.exceptPrefix(pfx.size());

    using seconds = std::chrono::duration<double>;
    const auto t = std::chrono::duration_cast<seconds>(clock.now() - t0).count();
    printf("[%10.03f] ", t);

    switch (Severity{severity})
    {
    using enum GL::DebugOutput::Severity;
    case Notification: std::fputs("DEBUG ", stdout); break;
    case Low: std::fputs("INFO  ", stdout); break;
    case Medium: std::fputs("NOTICE ", stdout); break;
    case High: std::fputs("ERROR ", stdout); break;
    default: std::fputs("????? ", stdout); break;
    }
    printf("%6u ", id);

    std::fwrite(str.data(), str.size(), 1, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
    std::fputs("", stdout); // put breakpoint here

#if 0
    if (severity != Severity::Notification)
        std::abort();
#endif

    //debug_break();
    gl_debug_put_breakpoint_here();
}

static void _debug_callback(GL::DebugOutput::Source src, GL::DebugOutput::Type type, UnsignedInt id,
                            GL::DebugOutput::Severity severity, StringView str, const void* self)
{
    static_cast<const main_impl*>(self)->debug_callback((unsigned)src, (unsigned)type, (unsigned)id, (unsigned)severity, str);
}

void main_impl::register_debug_callback()
{
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other, {131185}, false); // nvidia krap
    GL::DebugOutput::setCallback(_debug_callback, this);
}

} // namespace floormat
