#include "log.hpp"
#include <mg/Context.h>
#include <cstdlib>
#include <cstring>
#include <cr/String.h>

namespace floormat {

namespace {

StringView env_log_string()
{
    static const String str = [] {
        const auto* s = std::getenv("MAGNUM_LOG");
        return s && *s ? String{s} : String{};
    }();
    return str;
}

bool env_log_is(StringView level)
{
    return env_log_string() == level;
}

bool env_log_quiet()
{
    static const bool value = env_log_is("quiet"_s);
    return value;
}

bool env_log_verbose()
{
    static const bool value = env_log_is("verbose"_s);
    return value;
}

} // namespace

// Without a context, as under floormat-test --no-gl, MAGNUM_LOG sets the level and --magnum-log is ignored.
bool is_log_quiet()
{
    if (!GL::Context::hasCurrent())
        return env_log_quiet();
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::QuietLog);
}

bool is_log_verbose()
{
    if (!GL::Context::hasCurrent())
        return env_log_verbose();
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::VerboseLog);
}

bool is_log_standard()
{
    if (!GL::Context::hasCurrent())
        return !env_log_quiet() && !env_log_verbose();
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !(flags & (GLCCF::VerboseLog|GLCCF::QuietLog));
}

} // namespace floormat
