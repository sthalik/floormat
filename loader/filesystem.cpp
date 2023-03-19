#include "impl.hpp"
#include "compat/assert.hpp"
#include <cerrno>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Implementation/ErrorString.h>
#include <Corrade/Utility/Path.h>
#ifdef _WIN32
#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/Unicode.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace floormat::loader_detail {

#ifdef _WIN32
namespace Unicode = Corrade::Utility::Unicode;
#endif

bool loader_impl::chdir(StringView pathname)
{
    fm_assert(pathname.flags() & StringViewFlag::NullTerminated);
    int ret;
#ifdef _WIN32
    ret = ::_wchdir(Unicode::widen(pathname));
#else
    ret = ::chdir(pathname.data());
#endif
    if (ret)
    {
        Error err;
        err << "chdir: can't change directory to" << pathname << Error::nospace << ":";
        Corrade::Utility::Implementation::printErrnoErrorString(err, errno);
    }
    return !ret;
}

StringView loader_impl::startup_directory() noexcept
{
    fm_debug_assert(!original_working_directory.isEmpty());
    return original_working_directory;
}

void loader_impl::set_application_working_directory()
{
    static bool once = false;
    if (once)
        return;
    once = true;
    if (auto loc = Path::currentDirectory(); loc)
        original_working_directory = std::move(*loc);
    else
    {
        Error err; err << "can't get original working directory:";
        Corrade::Utility::Implementation::printErrnoErrorString(err, errno);
        original_working_directory = "."_s;
    }
    if (const auto loc = Path::executableLocation())
    {
        String path;
#ifdef _WIN32
        path = "\\\\?\\"_s + *loc;
#else
        path = *loc;
#endif
        StringView p = path;
        p = Path::split(p).first();
        p = Path::split(p).first();
        path = p;
#ifdef _WIN32
        for (char& c : path)
            if (c == '/')
                c = '\\';
#endif
        chdir(path) &&
        chdir("share/floormat"_s);
    }
    else
        fm_warn("can't find install prefix!");
}

} // namespace floormat::loader_detail
