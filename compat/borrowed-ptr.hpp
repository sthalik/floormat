#pragma once
#include "borrowed-ptr-fwd.hpp"

namespace floormat::detail_borrowed_ptr {

struct control_block;

template<typename From, typename To>
concept StaticCastable = requires(From* from) {
    static_cast<To*>(from);
};

template<typename From, typename To>
concept DerivedFrom = requires(From* x) {
    !std::is_same_v<From, To>;
    std::is_convertible_v<From*, To*>;
};

} // namespace floormat::detail_borrowed_ptr

namespace floormat {

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

    template<typename Y>
    requires std::is_convertible_v<T*, const bptr_base*>
    bptr(const bptr<Y>& other, std::nullptr_t) noexcept;

    template<typename Y>
    requires std::is_convertible_v<T*, const bptr_base*>
    bptr(bptr<Y>&& other, std::nullptr_t) noexcept;

    template<typename Y> bptr& _copy_assign(const bptr<Y>& other) noexcept;
    template<typename Y> bptr& _move_assign(bptr<Y>&& other) noexcept;

public:
    template<typename... Ts>
    requires (std::is_constructible_v<std::remove_const_t<T>, Ts&&...> && std::is_convertible_v<T*, const bptr_base*>)
    explicit bptr(InPlaceInitT, Ts&&... args) noexcept;

    explicit bptr(T* ptr) noexcept requires std::is_convertible_v<T*, const bptr_base*>;
    bptr() noexcept requires std::is_convertible_v<T*, const bptr_base*>;
    ~bptr() noexcept;

    bptr(std::nullptr_t) noexcept requires std::is_convertible_v<T*, const bptr_base*>; // NOLINT(*-explicit-conversions)
    bptr& operator=(std::nullptr_t) noexcept requires std::is_convertible_v<T*, const bptr_base*>;

    bptr(const bptr&) noexcept requires std::is_convertible_v<T*, const bptr_base*>;
    bptr& operator=(const bptr&) noexcept requires std::is_convertible_v<T*, const bptr_base*>;

    template<detail_borrowed_ptr::DerivedFrom<T> Y> bptr(const bptr<Y>&) noexcept
    requires std::is_convertible_v<T*, const bptr_base*>;

    template<detail_borrowed_ptr::DerivedFrom<T> Y> bptr& operator=(const bptr<Y>&) noexcept
    requires std::is_convertible_v<T*, const bptr_base*>;

    bptr(bptr&&) noexcept requires std::is_convertible_v<T*, const bptr_base*>;
    bptr& operator=(bptr&&) noexcept requires std::is_convertible_v<T*, const bptr_base*>;

    template<detail_borrowed_ptr::DerivedFrom<T> Y> bptr(bptr<Y>&&) noexcept
    requires std::is_convertible_v<T*, const bptr_base*>;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> bptr& operator=(bptr<Y>&&) noexcept
    requires std::is_convertible_v<T*, const bptr_base*>;

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

    template<typename To, typename From>
    requires (std::is_convertible_v<To*, const bptr_base*> && detail_borrowed_ptr::StaticCastable<From, To>)
    friend bptr<To> static_pointer_cast(const bptr<From>& p) noexcept;
};

template<typename To, typename From>
requires (std::is_convertible_v<To*, const bptr_base*> && detail_borrowed_ptr::StaticCastable<From, To>)
bptr<To> static_pointer_cast(const bptr<From>& p) noexcept
{
    if (p.blk && p.blk->_ptr) [[likely]]
        return bptr<To>{p, nullptr};
    return bptr<To>{nullptr};
}

} // namespace floormat
