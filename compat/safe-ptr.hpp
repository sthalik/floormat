#pragma once
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include <type_traits>
#include <Corrade/Tags.h>
#include <Corrade/Utility/Move.h>

namespace floormat {

template<typename T>
class safe_ptr final
{
    T* ptr;

public:
    template<typename... Ts>
    requires requires (Ts&&... xs) {
        new T{Utility::forward<Ts>(xs)...};
    }
    safe_ptr(InPlaceInitT, Ts&&... args) noexcept:
        ptr{new T{Utility::forward<Ts>(args)...}}
    {}

    explicit safe_ptr(T*&& ptr) noexcept: ptr{ptr}
    {
        fm_assert(ptr != nullptr);
    }

    ~safe_ptr() noexcept
    {
        if (ptr)
            delete ptr;
        ptr = (T*)-0xbadbabe;
    }

    explicit safe_ptr(safe_ptr&& other) noexcept: ptr{other.ptr}
    {
        other.ptr = nullptr;
    }

    safe_ptr& operator=(safe_ptr&& other) noexcept
    {
        fm_assert(this != &other);
        if (ptr)
            delete ptr;
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(safe_ptr);

    //explicit operator bool() const noexcept { return ptr != nullptr; }

    const T& operator*() const noexcept { return *ptr; }
    T& operator*() noexcept { return *ptr; }
    const T* operator->() const noexcept { return ptr; }
    T* operator->() noexcept { return ptr; }
};

} // namespace floormat
