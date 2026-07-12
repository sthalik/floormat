#pragma once
#include "compat/assert.hpp"
#include <type_traits>
#include <cr/Tags.h>

namespace floormat {

template<typename T>
class safe_ptr final
{
    T* ptr;

public:
    ~safe_ptr() noexcept
    {
        delete ptr;
        ptr = (T*)0xbadcafedeadbabe;
    }

    safe_ptr(std::nullptr_t) = delete;
    explicit safe_ptr(T* ptr) noexcept: ptr{ptr} { fm_assert(ptr != nullptr); }
    safe_ptr(safe_ptr&& other) noexcept: ptr{other.ptr} { other.ptr = nullptr; }

    safe_ptr() noexcept(noexcept(safe_ptr{InPlaceInit})): safe_ptr{InPlaceInit} {}

    template<typename... Ts>
    safe_ptr(InPlaceInitT, Ts&&... args) noexcept(std::is_nothrow_constructible_v<T, Ts&&...>):
        ptr(new T{ forward<Ts>(args)... })
    {}

    safe_ptr(const safe_ptr& other) noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        fm_assert(other.ptr); // copying a moved-from safe_ptr breaks the never-null invariant
        ptr = new T{*other.ptr};
    }

    safe_ptr& operator=(safe_ptr&& other) noexcept
    {
        fm_assert(this != &other);
        delete ptr;
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }

    safe_ptr& operator=(const safe_ptr& other) noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        fm_assert(other.ptr);
        if (ptr != other.ptr)
        {
            T* p = new T{*other.ptr}; // may throw; keep ptr valid until it succeeds
            delete ptr;
            ptr = p;
        }
        return *this;
    }

    //explicit operator bool() const noexcept { return ptr != nullptr; }

    T* get() noexcept { return ptr; }
    const T* get() const noexcept { return ptr; }
    const T& operator*() const noexcept { return *ptr; }
    T& operator*() noexcept { return *ptr; }
    const T* operator->() const noexcept { return ptr; }
    T* operator->() noexcept { return ptr; }
};

} // namespace floormat
