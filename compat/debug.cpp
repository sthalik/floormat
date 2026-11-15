#include "debug.hpp"
#include "strerror.hpp"
#include "format.inl"
#include <cerrno>
#include <iostream>
#include <cr/StringView.h>

// Error{} << "error:" << "can't open file" << quoted("foo") << error_string(EINVAL);
// ===> "error: can't open file 'foo': Invalid argument"

namespace floormat::detail::corrade_debug {

Debug& operator<<(Debug& dbg, ErrorString box)
{
    char buf[256];
    return dbg << nospace(":") << get_error_string(buf, box.value);
}

Debug& operator<<(Debug& dbg, Fraction f)
{
    char buf[56];
    snformat(buf, "{:.{}f}", (double)f.value, f.decimal_points);
    return dbg << buf;
}

} // namespace floormat::detail::corrade_debug

namespace floormat {

std::istream* standard_input() { return &std::cin; }
std::ostream* standard_output() { return &std::cout; }
std::ostream* standard_error() { return &std::cerr; }

using namespace floormat::detail::corrade_debug;

ErrorString error_string(int error) { return { error }; }
ErrorString error_string() { return { errno }; }

Fraction fraction(float value, int decimal_points) { return Fraction { value, decimal_points }; }

} // namespace floormat
