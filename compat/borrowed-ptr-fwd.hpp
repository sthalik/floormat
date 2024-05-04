#pragma once

namespace floormat::detail_borrowed_ptr { struct control_block_; }

namespace floormat {

template<typename T>
class bptr final // NOLINT(*-special-member-functions)
{
    mutable T* ptr;
    detail_borrowed_ptr::control_block_* blk;

    explicit constexpr bptr(NoInitT) noexcept;
    explicit constexpr bptr(DirectInitT, T* ptr, detail_borrowed_ptr::control_block_* blk) noexcept;

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

    void swap(bptr& other) noexcept;
    constexpr T* get() const noexcept;
    constexpr T& operator*() const noexcept;
    constexpr T* operator->() const noexcept;
    uint32_t use_count() const noexcept;
    explicit operator bool() const noexcept;

    template<typename U> friend class bptr;
    template<typename Tʹ, typename U> friend bptr<U> static_pointer_cast(const bptr<Tʹ>& p) noexcept;
};

} // namespace floormat
