#pragma once
#include "borrowed-ptr-fwd.hpp"
#include "borrowed-ptr-policy.hpp"

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
} // namespace floormat

namespace floormat::detail::bptr {

template<typename Policy> struct control_block;

template<typename From, typename To>
concept StaticCastable = requires(From* from, To* to) {
    static_cast<To*>(from);
    static_cast<const bptr_base*>(from);
    static_cast<const bptr_base*>(to);
};

template<typename From, typename To>
concept DerivedFrom = requires(From* from, To* to) {
    requires std::is_convertible_v<From&, To&>;
};

template<typename Y, typename T>
concept ComparableWith = requires(Y* y, T* t) { y == t; };

} // namespace floormat::detail::bptr

namespace floormat {

template<typename To, typename From, typename Policy> requires detail::bptr::StaticCastable<From, To>
basic_bptr<To, Policy> static_pointer_cast(basic_bptr<From, Policy>&& p) noexcept;

template<typename To, typename From, typename Policy> requires detail::bptr::StaticCastable<From, To>
basic_bptr<To, Policy> static_pointer_cast(const basic_bptr<From, Policy>& p) noexcept;

template<typename T, typename Policy>
class basic_bptr final // NOLINT(*-special-member-functions)
{
    using block = detail::bptr::control_block<Policy>;
    using count_type = Policy::counter::value_type;

    block* blk;

    template<typename Y> basic_bptr(const basic_bptr<Y, Policy>& other, std::nullptr_t) noexcept;
    template<typename Y> basic_bptr(basic_bptr<Y, Policy>&& other, std::nullptr_t) noexcept;
    template<typename Y> basic_bptr& _copy_assign(const basic_bptr<Y, Policy>& other) noexcept;
    template<typename Y> basic_bptr& _move_assign(basic_bptr<Y, Policy>&& other) noexcept;

public:
    template<typename... Ts>
    //requires std::is_constructible_v<std::remove_const_t<T>, Ts&&...>
    explicit basic_bptr(InPlaceInitT, Ts&&... args) noexcept(noexcept(std::remove_const_t<T>{std::declval<Ts>()...}));

    explicit basic_bptr(T* ptr) noexcept;
    basic_bptr() noexcept;
    basic_bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    ~basic_bptr() noexcept;

    basic_bptr& operator=(std::nullptr_t) noexcept;

    basic_bptr(const basic_bptr<std::remove_const_t<T>, Policy>& ptr) noexcept requires std::is_const_v<T>;
    basic_bptr(basic_bptr<std::remove_const_t<T>, Policy>&& ptr) noexcept requires std::is_const_v<T>;

    basic_bptr(const basic_bptr&) noexcept;
    basic_bptr& operator=(const basic_bptr&) noexcept;
    template<detail::bptr::DerivedFrom<T> Y> basic_bptr(const basic_bptr<Y, Policy>&) noexcept;
    template<detail::bptr::DerivedFrom<T> Y> basic_bptr& operator=(const basic_bptr<Y, Policy>&) noexcept;

    basic_bptr(basic_bptr&&) noexcept;
    basic_bptr& operator=(basic_bptr&&) noexcept;
    template<detail::bptr::DerivedFrom<T> Y> basic_bptr(basic_bptr<Y, Policy>&&) noexcept;
    template<detail::bptr::DerivedFrom<T> Y> basic_bptr& operator=(basic_bptr<Y, Policy>&&) noexcept;

    void reset() noexcept;
    template<detail::bptr::DerivedFrom<T> Y> void reset(Y* ptr) noexcept;
    void destroy() noexcept;
    void swap(basic_bptr& other) noexcept;
    count_type use_count() const noexcept;
    // true after destroy() through another copy, unlike operator bool
    bool has_block() const noexcept;

    T* get() const noexcept;
    T* operator->() const noexcept;
    T& operator*() const noexcept;

    explicit operator bool() const noexcept;

    bool operator==(const basic_bptr<const T, Policy>& other) const noexcept;
    bool operator==(const basic_bptr<T, Policy>& other) const noexcept requires (!std::is_const_v<T>);
    template<detail::bptr::ComparableWith<T> Y> bool operator==(const basic_bptr<Y, Policy>& other) const noexcept;
    bool operator==(std::nullptr_t) const noexcept;

    template<typename U, typename P> friend class basic_bptr;
    template<typename U, typename P> friend class basic_weak_bptr;

    template<typename To, typename From, typename P>
    requires detail::bptr::StaticCastable<From, To>
    friend basic_bptr<To, P> static_pointer_cast(basic_bptr<From, P>&& p) noexcept;

    template<typename To, typename From, typename P>
    requires detail::bptr::StaticCastable<From, To>
    friend basic_bptr<To, P> static_pointer_cast(const basic_bptr<From, P>& p) noexcept;
};

} // namespace floormat
