#include "debug.hpp"
#include "compat/strerror.hpp"
#include <cerrno>
#include <Corrade/Containers/StringView.h>

// Error{} << "error" << colon() << "can't open file" << colon() << quoted("foo") << error_string(EINVAL);
// ===> "error: can't open file 'foo': Invalid argument"

namespace floormat::detail::corrade_debug {

Debug& operator<<(Debug& dbg, Colon box)
{
    auto flags = dbg.flags();
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << StringView{&box.c, 1};
    dbg.setFlags(flags);
    return dbg;
}

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

Debug::Flags quoted_begin(Debug& dbg, char c)
{
    auto flags = dbg.flags();
    dbg << "";
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << StringView{&c, 1};
    return flags;
}

Debug& quoted_end(Debug& dbg, Debug::Flags flags, char c)
{
    dbg << StringView{&c, 1};
    dbg.setFlags(flags);
    return dbg;
}

template struct Quoted<StringView>;

} // namespace floormat::detail::corrade_debug

namespace floormat {

using namespace floormat::detail::corrade_debug;

Colon colon(char c) { return Colon{c}; }
ErrorString error_string(int error) { return { error }; }
ErrorString error_string() { return { errno }; }

} // namespace floormat
