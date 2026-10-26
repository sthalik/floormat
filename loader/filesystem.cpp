#include "impl.hpp"
#include "compat/assert.hpp"
#include "compat/debug.hpp"
#include "compat/path.hpp"
#include <cr/String.h>
#ifdef _WIN32
#include <cr/Array.h>
#include <cr/Unicode.h>
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
    const auto wide = Unicode::widen(pathname);
    // MAX_PATH is 260 with the null terminator, which widen()'s size leaves out. _wchdir accepts
    // '/' after the "//?/" prefix and stores the cwd with '\'.
    if (wide.size() < 260)
        ret = ::_wchdir(wide.data());
    else
        ret = ::_wchdir(Unicode::widen(extended_length_path(pathname)).data());
#else
    ret = ::chdir(pathname.data());
#endif
    if (ret)
    {
        auto err = error_string();
        ERR << "chdir: can't change directory to" << quoted(pathname) << err;
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
    static bool once = false; // todo thread safety
    if (once)
        return;
    once = true;
    if (auto loc = current_directory(); !loc.isEmpty())
        original_working_directory = move(loc);
    else
    {
        ERR << "can't get original working directory";
        original_working_directory = "."_s;
    }
    if (const auto loc = executable_location(); !loc.isEmpty())
    {
        StringView p = loc;
        p = split(p).first();
        p = split(p).first();
        const String path = p;
        chdir(path) &&
        chdir("share/floormat"_s);
    }
    else
        fm_warn("can't find install prefix!");
}

} // namespace floormat::loader_detail
