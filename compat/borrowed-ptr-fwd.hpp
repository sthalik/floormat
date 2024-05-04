#pragma once

namespace floormat::detail_borrowed_ptr {

struct control_block_;

template<typename From, typename To>
concept StaticCastable = requires(From* ptr) {
    static_cast<To*>(ptr);
};

} // namespace floormat::detail_borrowed_ptr

namespace floormat {

template<typename T>
class bptr final // NOLINT(*-special-member-functions)
{
    T* ptr; // todo add simple_bptr that doesn't allow casting. should only have the control block member variable.
    detail_borrowed_ptr::control_block_* blk;

public:
    template<typename... Ts>
    requires std::is_constructible_v<T, Ts&&...>
    explicit bptr(InPlaceInitT, Ts&&... args) noexcept;

    constexpr bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    constexpr bptr() noexcept;
    explicit constexpr bptr(NoInitT) noexcept;
    explicit bptr(T* ptr) noexcept;
    ~bptr() noexcept;

    template<typename Y> requires std::is_convertible_v<Y*, T*> bptr(const bptr<Y>&) noexcept;
    template<typename Y> requires std::is_convertible_v<Y*, T*> bptr(bptr<Y>&&) noexcept;
    template<typename Y> requires std::is_convertible_v<Y*, T*> bptr& operator=(const bptr<Y>&) noexcept;
    template<typename Y> requires std::is_convertible_v<Y*, T*> bptr& operator=(bptr<Y>&&) noexcept;

    void swap() noexcept;
    T* get() noexcept;
    const T* get() const noexcept;
    T& operator*() const noexcept;
    T* operator->() const noexcept;
    uint32_t use_count() const noexcept;
    explicit operator bool() const noexcept;

    template<typename U>
    requires detail_borrowed_ptr::StaticCastable<T, U>
    bptr<U> static_pointer_cast() noexcept;
};

} // namespace floormat
