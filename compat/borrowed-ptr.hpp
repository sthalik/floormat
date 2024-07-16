#pragma once
#include "borrowed-ptr-fwd.hpp"
#include <compare>

namespace floormat { struct bptr_base; }

namespace floormat::detail_bptr {

struct control_block final
{
    bptr_base* _ptr;
    uint32_t _count;
    static void decrement(control_block*& blk) noexcept;
};

template<typename From, typename To>
concept StaticCastable = requires(From* from) {
    static_cast<To*>(from);
};

template<typename From, typename To>
concept DerivedFrom = requires(From* x) {
    std::is_convertible_v<From*, To*>;
    std::is_convertible_v<From*, const bptr_base*>;
};

} // namespace floormat::detail_bptr

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
    detail_bptr::control_block* blk;

    template<typename Y> bptr(const bptr<Y>& other, std::nullptr_t) noexcept;
    template<typename Y> bptr(bptr<Y>&& other, std::nullptr_t) noexcept;
    template<typename Y> bptr& _copy_assign(const bptr<Y>& other) noexcept;
    template<typename Y> bptr& _move_assign(bptr<Y>&& other) noexcept;

public:
    template<typename... Ts>
    requires std::is_constructible_v<std::remove_const_t<T>, Ts&&...>
    explicit bptr(InPlaceInitT, Ts&&... args) noexcept;

    template<detail_bptr::DerivedFrom<T> Y> explicit bptr(Y* ptr) noexcept;
    bptr() noexcept;
    ~bptr() noexcept;

    bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    bptr& operator=(std::nullptr_t) noexcept;

    bptr(const bptr&) noexcept;
    bptr& operator=(const bptr&) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> bptr(const bptr<Y>&) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> bptr& operator=(const bptr<Y>&) noexcept;

    bptr(bptr&&) noexcept;
    bptr& operator=(bptr&&) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> bptr(bptr<Y>&&) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> bptr& operator=(bptr<Y>&&) noexcept;

    void reset() noexcept;
    template<detail_bptr::DerivedFrom<T> Y> void reset(Y* ptr) noexcept;
    void destroy() noexcept;
    void swap(bptr& other) noexcept;
    uint32_t use_count() const noexcept;

    T* get() const noexcept;
    T* operator->() const noexcept;
    T& operator*() const noexcept;

    explicit operator bool() const noexcept;

    bool operator==(const bptr<const std::remove_const_t<T>>& other) const noexcept;
    bool operator==(const bptr<std::remove_const_t<T>>& other) const noexcept;
    bool operator==(const std::nullptr_t& other) const noexcept;

    std::strong_ordering operator<=>(const bptr<const std::remove_const_t<T>>& other) const noexcept;
    std::strong_ordering operator<=>(const bptr<std::remove_const_t<T>>& other) const noexcept;
    std::strong_ordering operator<=>(const std::nullptr_t& other) const noexcept;

    template<typename U> friend class bptr;

    template<typename To, typename From>
    requires detail_bptr::StaticCastable<From, To>
    friend bptr<To> static_pointer_cast(const bptr<From>& p) noexcept;
};

template<typename To, typename From>
requires detail_bptr::StaticCastable<From, To>
bptr<To> static_pointer_cast(const bptr<From>& p) noexcept
{
    if (p.blk && p.blk->_ptr) [[likely]]
        return bptr<To>{p};
    return bptr<To>{nullptr};
}

} // namespace floormat
