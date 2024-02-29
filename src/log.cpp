#include "log.hpp"
#include <mg/Context.h>

namespace floormat {

bool is_log_quiet()
{
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::QuietLog);
}

bool is_log_verbose()
{
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::VerboseLog);
}

bool is_log_standard()
{
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !(flags & (GLCCF::VerboseLog|GLCCF::QuietLog));
}

} // namespace floormat
