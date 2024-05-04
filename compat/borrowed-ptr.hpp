#pragma once

namespace floormat::detail_borrowed_ptr {
struct control_block_;
} // namespace floormat::detail_borrowed_ptr

namespace floormat {
template<typename T> class bptr;
template<typename T> bptr<T> static_pointer_cast(const bptr<T>& p);

template<typename T>
class bptr final // NOLINT(*-special-member-functions)
{
    T* ptr; // todo add simple_bptr that doesn't allow casting. should only have the control block element.
    detail_borrowed_ptr::control_block_* blk;

    constexpr bptr(NoInitT) noexcept;

public:
    template<typename... Ts>
    requires std::is_constructible_v<T, Ts&&...>
    explicit bptr(InPlaceInitT, Ts&&... args) noexcept;

    constexpr bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    constexpr bptr() noexcept;
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

    friend bptr<T> static_pointer_cast<T>(const bptr<T>& p);
};

template<typename T> constexpr bptr<T>::bptr(NoInitT) noexcept {};
template<typename T> constexpr bptr<T>::bptr(std::nullptr_t) noexcept: ptr{nullptr}, blk{nullptr} {}
template<typename T> constexpr bptr<T>::bptr() noexcept: bptr{nullptr} {}


template<typename T> bptr(T* ptr) -> bptr<T>;

} // namespace floormat
