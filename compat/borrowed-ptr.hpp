#pragma once
#include "borrowed-ptr-fwd.hpp"

namespace floormat::detail_borrowed_ptr {

struct control_block;
template<typename From, typename To>
concept DerivedFrom = requires(From* x) {
    requires !std::is_same_v<From, To>;
    requires std::is_nothrow_convertible_v<From*, To*>;
};

} // namespace floormat::detail_borrowed_ptr

namespace floormat {

template<typename T> class bptr;

struct bptr_base
{
    virtual ~bptr_base() noexcept;
    bptr_base() noexcept;
    bptr_base(const bptr_base&) noexcept;
    bptr_base(bptr_base&&) noexcept;
    bptr_base& operator=(const bptr_base&) noexcept;
    bptr_base& operator=(bptr_base&&) noexcept;
};

template<typename T>
class bptr final // NOLINT(*-special-member-functions)
{
    detail_borrowed_ptr::control_block* blk;

    template<typename Y> bptr(const bptr<Y>& other, std::nullptr_t) noexcept;
    template<typename Y> bptr& _copy_assign(const bptr<Y>& other) noexcept;
    template<typename Y> bptr(bptr<Y>&& other, std::nullptr_t) noexcept;
    template<typename Y> bptr& _move_assign(bptr<Y>&& other) noexcept;

public:
    template<typename... Ts>
    requires std::is_constructible_v<std::remove_const_t<T>, Ts&&...>
    explicit bptr(InPlaceInitT, Ts&&... args) noexcept;

    explicit bptr(T* ptr) noexcept;
    bptr() noexcept;
    ~bptr() noexcept;

    bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    bptr& operator=(std::nullptr_t) noexcept;

    CORRADE_ALWAYS_INLINE bptr(const bptr&) noexcept;
    CORRADE_ALWAYS_INLINE bptr& operator=(const bptr&) noexcept;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> CORRADE_ALWAYS_INLINE bptr(const bptr<Y>&) noexcept;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> CORRADE_ALWAYS_INLINE bptr& operator=(const bptr<Y>&) noexcept;

    CORRADE_ALWAYS_INLINE bptr(bptr&&) noexcept;
    CORRADE_ALWAYS_INLINE bptr& operator=(bptr&&) noexcept;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> CORRADE_ALWAYS_INLINE bptr(bptr<Y>&&) noexcept;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> CORRADE_ALWAYS_INLINE bptr& operator=(bptr<Y>&&) noexcept;

    operator bptr<const T>() const noexcept requires (!std::is_const_v<T>);

    void reset() noexcept;
    void destroy() noexcept;
    void swap(bptr& other) noexcept;
    uint32_t use_count() const noexcept;

    T* get() const noexcept;
    T* operator->() const noexcept;
    T& operator*() const noexcept;

    explicit operator bool() const noexcept;
    bool operator==(const bptr<const std::remove_const_t<T>>& other) const noexcept;
    bool operator==(const bptr<std::remove_const_t<T>>& other) const noexcept;

    template<typename U> friend class bptr;
    template<typename U, typename Tʹ> friend bptr<U> static_pointer_cast(const bptr<Tʹ>& p) noexcept;
};

} // namespace floormat
