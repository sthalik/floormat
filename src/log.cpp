#include "log.hpp"
#include <mg/Context.h>

namespace floormat {

// Without a context, as under floormat-test --no-gl, the log level is the standard one.
bool is_log_quiet()
{
    if (!GL::Context::hasCurrent())
        return false;
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::QuietLog);
}

bool is_log_verbose()
{
    if (!GL::Context::hasCurrent())
        return false;
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::VerboseLog);
}

bool is_log_standard()
{
    if (!GL::Context::hasCurrent())
        return true;
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !(flags & (GLCCF::VerboseLog|GLCCF::QuietLog));
}

} // namespace floormat
