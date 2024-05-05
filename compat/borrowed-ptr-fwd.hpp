#pragma once

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
template<typename T> bool operator==(const bptr<T>& a, const bptr<T>& b) noexcept;

template<typename T>
class bptr final // NOLINT(*-special-member-functions)
{
    mutable T* casted_ptr;
    detail_borrowed_ptr::control_block* blk;

    explicit bptr(DirectInitT, T* casted_ptr, detail_borrowed_ptr::control_block* blk) noexcept;

    struct private_tag_t final {};
    static constexpr private_tag_t private_tag{};

    template<typename Y> bptr(const bptr<Y>& other, private_tag_t) noexcept;
    template<typename Y> bptr& _copy_assign(const bptr<Y>& other) noexcept;
    template<typename Y> bptr(bptr<Y>&& other, private_tag_t) noexcept;
    template<typename Y> bptr& _move_assign(bptr<Y>&& other) noexcept;

public:
    template<typename... Ts>
    requires std::is_constructible_v<T, Ts&&...>
    explicit bptr(InPlaceInitT, Ts&&... args) noexcept;

    explicit bptr(T* ptr) noexcept;
    bptr() noexcept;
    ~bptr() noexcept;

    bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    bptr& operator=(std::nullptr_t) noexcept;

    bptr(const bptr&) noexcept;
    bptr& operator=(const bptr&) noexcept;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> bptr(const bptr<Y>&) noexcept;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> bptr& operator=(const bptr<Y>&) noexcept;

    bptr(bptr&&) noexcept;
    bptr& operator=(bptr&&) noexcept;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> bptr(bptr<Y>&&) noexcept;
    template<detail_borrowed_ptr::DerivedFrom<T> Y> bptr& operator=(bptr<Y>&&) noexcept;

    void reset() noexcept;
    template<bool MaybeEmpty = true> void destroy() noexcept;
    void swap(bptr& other) noexcept;
    uint32_t use_count() const noexcept;

    T* get() const noexcept;
    T* operator->() const noexcept;
    T& operator*() const noexcept;

    explicit operator bool() const noexcept;
    friend bool operator==<T>(const bptr<T>& a, const bptr<T>& b) noexcept;

    template<typename U> friend class bptr;
    template<typename U, typename Tʹ> friend bptr<U> static_pointer_cast(const bptr<Tʹ>& p) noexcept;
};

template<typename T> bptr(T* ptr) -> bptr<T>;

} // namespace floormat
