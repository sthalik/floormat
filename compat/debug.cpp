#include "debug.hpp"
#include "compat/strerror.hpp"

namespace floormat::detail::corrade_debug {

Debug& operator<<(Debug& dbg, ErrorString box)
{
    auto flags = dbg.flags();
    char buf[256];
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << ": ";
    dbg << get_error_string(buf, box.value);
    dbg.setFlags(flags);
    return dbg;
}

template struct Quoted<StringView>;

Debug::Flags debug1(Debug& dbg, char c)
{
    auto flags = dbg.flags();
    dbg << "";
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    char buf[2] { c, '\0' };
    dbg << buf;
    return flags;
}

Debug& debug2(Debug& dbg, Debug::Flags flags, char c)
{
    char buf[2] { c, '\0' };
    dbg << buf;
    dbg.setFlags(flags);
    return dbg;
}

} // namespace floormat::detail::corrade_debug

namespace floormat {

floormat::detail::corrade_debug::ErrorString error_string(int error) { return { error }; }

} // namespace floormat
