#pragma once

namespace floormat::detail_borrowed_ptr { struct control_block_; }

namespace floormat {

template<typename T> class bptr;
template<typename T> bool operator==(const bptr<T>& a, const bptr<T>& b) noexcept;

template<typename T>
class bptr final // NOLINT(*-special-member-functions)
{
    mutable T* casted_ptr;
    detail_borrowed_ptr::control_block_* blk;

    explicit bptr(DirectInitT, T* casted_ptr, detail_borrowed_ptr::control_block_* blk) noexcept;
    //explicit bptr(NoInitT) noexcept;

public:
    template<typename... Ts>
    requires std::is_constructible_v<T, Ts&&...>
    explicit bptr(InPlaceInitT, Ts&&... args) noexcept;

    bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    bptr() noexcept;
    explicit bptr(T* ptr) noexcept;
    ~bptr() noexcept;

    bptr& operator=(std::nullptr_t) noexcept;
    template<typename Y> requires std::is_convertible_v<Y*, T*> bptr(const bptr<Y>&) noexcept;
    template<typename Y> requires std::is_convertible_v<Y*, T*> bptr& operator=(const bptr<Y>&) noexcept;
    template<typename Y> requires std::is_convertible_v<Y*, T*> bptr(bptr<Y>&&) noexcept;
    template<typename Y> requires std::is_convertible_v<Y*, T*> bptr& operator=(bptr<Y>&&) noexcept;

    friend bool operator==<T>(const bptr<T>& a, const bptr<T>& b) noexcept;
    explicit operator bool() const noexcept;
    void reset() noexcept;
    template<bool MaybeEmpty = true> void destroy() noexcept;
    void swap(bptr& other) noexcept;
    uint32_t use_count() const noexcept;

    T* get() const noexcept;
    T* operator->() const noexcept;
    T& operator*() const noexcept;

    template<typename U> friend class bptr;
    template<typename Tʹ, typename U> friend bptr<U> static_pointer_cast(const bptr<Tʹ>& p) noexcept;
};

} // namespace floormat
