#include "impl.hpp"
#include "compat/assert.hpp"
#include <cerrno>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Implementation/ErrorString.h>
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

bool chdir(StringView pathname)
{
    int ret;
#ifdef _WIN32
    ret = _wchdir(Unicode::widen(pathname));
#else
    ret = chdir(pathname.data());
#endif
    if (ret)
    {
        Error err;
        err << "chdir: can't change directory to" << pathname << Error::nospace << ':';
        Corrade::Utility::Implementation::printErrnoErrorString(err, errno);
    }
    return !ret;
}

} // namespace floormat::loader_detail
