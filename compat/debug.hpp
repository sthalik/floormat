#pragma once
#include <Corrade/Utility/Debug.h>
#include <Corrade/Containers/StringView.h>
#include <concepts>

namespace floormat {

template<typename T>
concept DebugPrintable = requires(Debug& dbg, const T& value) {
    { dbg << value } -> std::convertible_to<Debug&>;
};

} // namespace floormat

namespace floormat::detail::corrade_debug {

// ***** colon *****
struct Colon { char c; };
Debug& operator<<(Debug& dbg, Colon box);

// ***** error string *****
struct ErrorString { int value; };
Debug& operator<<(Debug& dbg, ErrorString box);

// ***** quoted *****
template<typename T> struct Quoted
{
    using type = T;
    T value; char c;
};

Debug::Flags quoted_begin(Debug& dbg, char c);
Debug& quoted_end(Debug& dbg, Debug::Flags flags, char c);

template<typename T> Debug& operator<<(Debug& dbg, Quoted<T> box)
{
    Debug::Flags flags = quoted_begin(dbg, box.c);
    dbg << box.value;
    return quoted_end(dbg, flags, box.c);
}

// ***** float *****

struct Fraction
{
    float value;
    uint8_t decimal_points;
};

Debug& operator<<(Debug& dbg, Fraction frac);

} // namespace floormat::detail::corrade_debug

// ***** api *****

// ***** functions *****

namespace floormat {

floormat::detail::corrade_debug::Colon colon(char c = ':');
floormat::detail::corrade_debug::ErrorString error_string(int error);
floormat::detail::corrade_debug::ErrorString error_string();

floormat::detail::corrade_debug::Fraction fraction(float value, uint8_t decimal_points = 1);

template<DebugPrintable T>
auto quoted(T&& value, char c = '\'')
{
    using U = std::remove_cvref_t<T>;
    using floormat::detail::corrade_debug::Quoted;
    if constexpr(std::is_rvalue_reference_v<decltype(value)>)
        return Quoted<U>{ .value = move(value), .c = c };
    else
        return Quoted<const U&>{ .value = value, .c = c };
}

template<DebugPrintable T> auto quoted2(T&& value) { return quoted(forward<T>(value), '"'); }

} // namespace floormat
