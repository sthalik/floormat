#pragma once
#include "defs.hpp"
#include "move.hpp"
#include <new>
#include <cr/Macros.h>

namespace floormat::detail {

struct construct_at_fn
{
    template<typename T, typename... Args>
    requires requires(T* p, Args&&... args) { ::new (static_cast<void*>(p)) T(forward<Args>(args)...); }
    static constexpr CORRADE_ALWAYS_INLINE T* operator()(T* p, Args&&... args)
        noexcept(noexcept(::new (static_cast<void*>(p)) T(forward<Args>(args)...)));
};

template<typename T, typename... Args>
requires requires(T* p, Args&&... args) { ::new (static_cast<void*>(p)) T(forward<Args>(args)...); }
constexpr T* construct_at_fn::operator()(T* p, Args&&... args)
    noexcept(noexcept(::new (static_cast<void*>(p)) T(forward<Args>(args)...)))
{
    [[fm_msvc_cx_placement_new]] return ::new (static_cast<void*>(p)) T(forward<Args>(args)...);
}

struct destroy_at_fn
{
    template<typename T>
    static constexpr CORRADE_ALWAYS_INLINE void operator()(T* p) noexcept(noexcept(p->~T()));
};

template<typename T>
constexpr void destroy_at_fn::operator()(T* p) noexcept(noexcept(p->~T()))
{
    p->~T();
}

} // namespace floormat::detail

namespace floormat {

constexpr inline detail::construct_at_fn construct_at{};
constexpr inline detail::destroy_at_fn destroy_at{};

} // namespace floormat
