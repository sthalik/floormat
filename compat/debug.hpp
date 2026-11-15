#pragma once
#include <cr/Debug.h>
#include <concepts>

namespace floormat {

template<typename T>
concept DebugPrintable = requires(Debug& dbg, const T& value) {
    { dbg << value } -> std::convertible_to<Debug&>;
};

} // namespace floormat

namespace floormat::detail::corrade_debug {

// ***** error string *****
struct ErrorString { int value; };
Debug& operator<<(Debug& dbg, ErrorString box);

// ***** quoted *****
template<typename T> struct Quoted { T value; char c[2]; };

// ***** nospace *****
template<typename F> struct NoSpace { F print; };
template<typename F> Debug& operator<<(Debug& dbg, const NoSpace<F>& box) { return box.print(dbg); }

// ***** float *****

struct Fraction
{
    float value;
    int decimal_points;
};

Debug& operator<<(Debug& dbg, Fraction frac);

} // namespace floormat::detail::corrade_debug

// ***** functions *****

namespace floormat {

std::istream* standard_input();
std::ostream* standard_output();
std::ostream* standard_error();

floormat::detail::corrade_debug::ErrorString error_string(int error);
floormat::detail::corrade_debug::ErrorString error_string();

floormat::detail::corrade_debug::Fraction fraction(float value, int decimal_points = 1);

template<DebugPrintable T, DebugPrintable... Ts>
auto cat(T&& value, Ts&&... values)
{
    auto print = [&v = value, &...vs = values](Debug& dbg) -> Debug& {
        dbg << v;
        ((dbg << Debug::nospace << vs), ...);
        return dbg;
    };
    return floormat::detail::corrade_debug::NoSpace<decltype(print)>{print};
}

template<DebugPrintable T, DebugPrintable... Ts>
auto nospace(T&& value, Ts&&... values)
{
    auto print = [&v = value, &...vs = values](Debug& dbg) -> Debug& {
        dbg << Debug::nospace << v;
        ((dbg << Debug::nospace << vs), ...);
        return dbg;
    };
    return floormat::detail::corrade_debug::NoSpace<decltype(print)>{print};
}

template<DebugPrintable T>
auto quoted(T&& value, char c = '\'')
{
    return floormat::detail::corrade_debug::Quoted<T&&>{ .value = floormat::forward<T>(value), .c = { c, '\0' } };
}

template<DebugPrintable T> auto quoted2(T&& value) { return quoted(forward<T>(value), '"'); }

} // namespace floormat

namespace floormat::detail::corrade_debug {

template<typename T> Debug& operator<<(Debug& dbg, const Quoted<T>& box)
{
    return dbg << floormat::cat(box.c, box.value, box.c);
}

} // namespace floormat::detail::corrade_debug
