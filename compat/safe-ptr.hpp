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
    ~safe_ptr() noexcept
    {
        delete ptr;
        ptr = (T*)0xbadcafedeadbabe;
    }

    safe_ptr(std::nullptr_t) = delete;
    safe_ptr(T* ptr) noexcept: ptr{ptr} { fm_assert(ptr != nullptr); }
    safe_ptr(safe_ptr&& other) noexcept: ptr{other.ptr} { other.ptr = nullptr; }

    safe_ptr() noexcept: safe_ptr{InPlaceInit} {}

    template<typename... Ts> safe_ptr(InPlaceInitT, Ts&&... args) noexcept:
        ptr(new T{ Utility::forward<Ts>(args)... })
    {}

    safe_ptr& operator=(safe_ptr&& other) noexcept
    {
        fm_assert(this != &other);
        delete ptr;
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(safe_ptr);

    //explicit operator bool() const noexcept { return ptr != nullptr; }

    T* get() noexcept { return ptr; }
    const T* get() const noexcept { return ptr; }
    const T& operator*() const noexcept { return *ptr; }
    T& operator*() noexcept { return *ptr; }
    const T* operator->() const noexcept { return ptr; }
    T* operator->() noexcept { return ptr; }
};

} // namespace floormat
