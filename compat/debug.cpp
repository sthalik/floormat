#include "debug.hpp"
#include "compat/strerror.hpp"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <Corrade/Containers/StringView.h>


// Error{} << "error" << colon() << "can't open file" << colon() << quoted("foo") << error_string(EINVAL);
// ===> "error: can't open file 'foo': Invalid argument"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

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

Debug& operator<<(Debug& dbg, Fraction f)
{
    char fmt[8], buf[56];
    std::snprintf(fmt, sizeof fmt, "%%.%uf", (unsigned)f.decimal_points);
    std::snprintf(buf, sizeof buf, fmt, (double)f.value);
    dbg << buf;
    return dbg;
}

} // namespace floormat::detail::corrade_debug

namespace floormat {

std::ostream* standard_output() { return &std::cout; }
std::ostream* standard_error() { return &std::cerr; }

using namespace floormat::detail::corrade_debug;

Colon colon(char c) { return Colon{c}; }
ErrorString error_string(int error) { return { error }; }
ErrorString error_string() { return { errno }; }

Fraction fraction(float value, int decimal_points) { return Fraction { value, decimal_points }; }

} // namespace floormat
