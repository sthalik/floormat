#include "main-impl.hpp"
#include "compat/nanosecond.hpp"
#include <cstdio>
#include <mg/Context.h>
#include <mg/Extensions.h>
#include <mg/Renderer.h>
#include <mg/DebugOutput.h>

namespace floormat {

using Severity = GL::DebugOutput::Severity;
using Type     = GL::DebugOutput::Type;
using Source   = GL::DebugOutput::Source;

namespace {

void _debug_callback(Source src, Type type, UnsignedInt id, Severity severity, StringView str, const void*)
{ main_impl::debug_callback((u32)src, (u32)type, id, (u32)severity, str); }

CORRADE_NEVER_INLINE void gl_debug_put_breakpoint_here()
{
    std::fflush(stdout);
}

void break_on_serious_message(Severity severity, Type type)
{
    switch (severity)
    {
    default:
        break;
    case Severity::High:
        //std::abort();
        gl_debug_put_breakpoint_here();
        return;
    }

    switch (type)
    {
    using enum Type;
    case Error:
    case DeprecatedBehavior:
    case UndefinedBehavior:
        gl_debug_put_breakpoint_here();
        return;
    case Performance:
    case Marker:
    case Other:
    case PopGroup:
    case Portability:
    case PushGroup:
        break;
    }
}

} // namespace

void main_impl::debug_callback(u32 src, u32 type, u32 id, u32 severity, StringView str)
{
    static const auto t0 = Time::now();
#if 1
    volatile const auto _id       = id;
    volatile const char* _str     = str.data();
    volatile const auto _type     = Type    {std::underlying_type_t<Type>(type)};
    volatile const auto _src      = Source  {std::underlying_type_t<Source>(src)};
    volatile const auto _severity = Severity{std::underlying_type_t<Severity>(severity)};
    (void)_type; (void)_id; (void)_src; (void)_severity; (void)_str;
#endif

    if (auto pfx = "Buffer detailed info: "_s; str.hasPrefix(pfx))
        str = str.exceptPrefix(pfx.size());

    const auto t = double((Time::now() - t0).stamp) * 1e-9;
    printf("[%10.03f] ", t);

    switch (Severity{std::underlying_type_t<Severity>(severity)})
    {
    using enum Severity;
    case Notification: std::fputs("DEBUG ", stdout); goto severity_ok;
    case Low: std::fputs("INFO  ", stdout); goto severity_ok;
    case Medium: std::fputs("NOTICE ", stdout); goto severity_ok;
    case High: std::fputs("ERROR ", stdout); goto severity_ok;
    }
    std::fputs("????? ", stdout);
severity_ok:
    printf("%6u ", id);


    std::fwrite(str.data(), str.size(), 1, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
    std::fputs("", stdout); // put breakpoint here

    break_on_serious_message(Severity{std::underlying_type_t<Severity>(severity)},
                             Type{std::underlying_type_t<Type>(type)});
}

void main_impl::register_debug_callback()
{
    static bool once = false;
    fm_assert(!once);
    once = true;

    GL::DebugOutput::setCallback(_debug_callback, nullptr);
    GL::DebugOutput::setEnabled(true);
    GL::DebugOutput::setEnabled(Source::Api, Type::Other, {131185, 131169}, false); // nvidia krap
#if 1 // hides debug group messages
    GL::DebugOutput::setEnabled(Source::Application, Type::PushGroup, false);
    GL::DebugOutput::setEnabled(Source::Application, Type::PopGroup, false);
#endif

    if (GL::Context::current().isExtensionSupported<GL::Extensions::KHR::debug>())
    {
        GL::Renderer::enable(GL::Renderer::Feature::DebugOutput);
        GL::Renderer::enable(GL::Renderer::Feature::DebugOutputSynchronous);
    }
}

} // namespace floormat
