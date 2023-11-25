#pragma once
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Move.h>

namespace floormat::detail::corrade_debug {

template<typename T> struct Quoted final { const T& value; };

#ifndef FM_NO_CORRADE_DEBUG_EXTERN_TEMPLATE_QUOTED
extern template struct Quoted<StringView>;
#endif

Debug::Flags debug1(Debug& dbg);
Debug& debug2(Debug& dbg, Debug::Flags flags);

template<typename T>
Debug& operator<<(Debug& dbg, detail::corrade_debug::Quoted<T> box)
{
    Debug::Flags flags = detail::corrade_debug::debug1(dbg);
    dbg << box.value;
    return debug2(dbg, flags);
}

} // namespace floormat::detail::corrade_debug

template<typename T> floormat::detail::corrade_debug::Quoted<T> quoted(const T& value) { return { value }; }
