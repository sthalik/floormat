#pragma once

namespace floormat::detail_type_traits {

namespace { template<class T> class IsComplete_ {
    // from <Corrade/Containers/Pointer.h>
    template<class U> static char get(U*, decltype(sizeof(U))* = nullptr);
    static short get(...);
    public:
        enum: bool { value = sizeof(get(static_cast<T*>(nullptr))) == sizeof(char) };
}; } // namespace

} // namespace floormat::detail_type_traits

namespace floormat {

template<typename T>
constexpr inline bool is_complete =
    bool(::floormat::detail_type_traits::IsComplete_<T>::value);

} // namespace floormat
