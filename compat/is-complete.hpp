#pragma once

namespace floormat {

namespace { template<class T> struct IsComplete {
    template<class U> static char get(U*, decltype(sizeof(U))* = nullptr);
    static short get(...);
    public:
        enum: bool { value = sizeof(get(static_cast<T*>(nullptr))) == sizeof(char) };
}; }

} // namespace floormat
