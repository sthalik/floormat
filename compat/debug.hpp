#pragma once
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Move.h>
#include <Corrade/Containers/Containers.h>

namespace floormat::detail::corrade_debug {

struct ErrorString final { int value; };
Debug& operator<<(Debug& dbg, ErrorString box);

template<typename T> struct Quoted final { const T& value; };
Debug::Flags debug1(Debug& dbg, char c);
Debug& debug2(Debug& dbg, Debug::Flags flags, char c);

template<typename T> Debug& operator<<(Debug& dbg, Quoted<T> box)
{
    Debug::Flags flags = debug1(dbg, '\'');
    dbg << box.value;
    return debug2(dbg, flags, '\'');
}

extern template struct Quoted<StringView>;

} // namespace floormat::detail::corrade_debug

namespace floormat {

floormat::detail::corrade_debug::ErrorString error_string(int error);
template<typename T> floormat::detail::corrade_debug::Quoted<T> quoted(const T& value) { return { value }; }

} // namespace floormat
