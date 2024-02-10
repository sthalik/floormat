#include "os-file.hpp"
#include "assert.hpp"
#include <cerrno>
#include <cr/StringView.h>

#ifdef _WIN32
#include <io.h>
#define fm_os_access _access
#else
#include <unistd.h>
#define fm_os_access access
#endif
#ifndef F_OK
#define F_OK 0
#endif

namespace floormat::fs {

bool file_exists(StringView name)
{
    fm_assert(name.flags() & StringViewFlag::NullTerminated);
    fm_debug_assert(!name.find('\0'));
    if (!fm_os_access(name.data(), F_OK))
        return true;
    int error = errno;
    // just let it die if the file exists but can't be accessed
    return error != ENOENT;
}

} // namespace floormat::fs
