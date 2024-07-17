#pragma once
#include "borrowed-ptr-fwd.hpp"

#define FM_BPTR_DEBUG
#define FM_NO_WEAK_BPTR

#ifdef __CLION_IDE__
#define fm_bptr_assert(...) (void(__VA_ARGS__))
#elif defined FM_BPTR_DEBUG && !defined FM_NO_DEBUG
#define fm_bptr_assert(...) fm_assert(__VA_ARGS__)
#else
#define fm_bptr_assert(...) void()
#endif

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

namespace floormat::detail_bptr {

struct control_block final
{
    bptr_base* _ptr;
#ifndef FM_NO_WEAK_BPTR
    uint32_t _soft_count;
#endif
    uint32_t _hard_count;
    static void decrement(control_block*& blk) noexcept;
#ifndef FM_NO_WEAK_BPTR
    static void weak_decrement(control_block*& blk) noexcept;
#endif
};

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

} // namespace floormat::detail_bptr

namespace floormat {

template<typename To, typename From> requires detail_bptr::StaticCastable<From, To>
bptr<To> static_pointer_cast(bptr<From>&& p) noexcept;

template<typename To, typename From> requires detail_bptr::StaticCastable<From, To>
bptr<To> static_pointer_cast(const bptr<From>& p) noexcept;

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
    //requires std::is_constructible_v<std::remove_const_t<T>, Ts&&...>
    CORRADE_ALWAYS_INLINE explicit bptr(InPlaceInitT, Ts&&... args) noexcept;

    CORRADE_ALWAYS_INLINE explicit bptr(T* ptr) noexcept;
    CORRADE_ALWAYS_INLINE bptr() noexcept;
    CORRADE_ALWAYS_INLINE bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    ~bptr() noexcept;

    bptr& operator=(std::nullptr_t) noexcept;

    bptr(const bptr<std::remove_const_t<T>>& ptr) noexcept requires std::is_const_v<T>;
    bptr(bptr<std::remove_const_t<T>>&& ptr) noexcept requires std::is_const_v<T>;

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

    bool operator==(const bptr<const T>& other) const noexcept;
    bool operator==(const bptr<T>& other) const noexcept requires (!std::is_const_v<T>);
    bool operator==(const std::nullptr_t& other) const noexcept;

    template<typename U> friend class bptr;
    template<typename U> friend class weak_bptr;

    template<typename To, typename From>
    requires detail_bptr::StaticCastable<From, To>
    friend bptr<To> static_pointer_cast(bptr<From>&& p) noexcept;

    template<typename To, typename From>
    requires detail_bptr::StaticCastable<From, To>
    friend bptr<To> static_pointer_cast(const bptr<From>& p) noexcept;
};

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

template<typename T>
template<typename... Ts>
//requires std::is_constructible_v<std::remove_const_t<T>, Ts&&...>
bptr<T>::bptr(InPlaceInitT, Ts&&... args) noexcept:
    bptr{ new std::remove_const_t<T>{ forward<Ts>(args)... } }
{}

template<typename T> bptr<T>::bptr(std::nullptr_t) noexcept: blk{nullptr} {}
template<typename T> bptr<T>::bptr() noexcept: bptr{nullptr} {}

template<typename T>
bptr<T>::bptr(T* ptr) noexcept:
    blk{ptr ? new detail_bptr::control_block{const_cast<std::remove_const_t<T>*>(ptr), 1,
#ifndef FM_NO_WEAK_BPTR
        1,
#endif
    } : nullptr}
{}

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif


} // namespace floormat
