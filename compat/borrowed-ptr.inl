#pragma once
#include "borrowed-ptr.hpp"
#include "compat/assert.hpp"

#define FM_BPTR_DEBUG

#if defined FM_BPTR_DEBUG && !defined FM_NO_DEBUG
#define fm_bptr_assert(...) fm_assert(__VA_ARGS__)
#else
#define fm_bptr_assert(...) void()
#endif

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace floormat::detail_borrowed_ptr {

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif
struct control_block_
{
    void* _ptr; // todo maybe add directly embeddable objects?
    uint32_t _count;
    virtual void free_ptr() noexcept = 0;
    static void decrement(control_block_*& blk) noexcept;
};
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

template<typename T>
struct control_block_impl final: control_block_
{
    void free_ptr() noexcept override;
    [[nodiscard]] static control_block_* create(T* ptr) noexcept;
protected:
    explicit control_block_impl(T* ptr) noexcept;
};

template <typename T>
control_block_impl<T>::control_block_impl(T* ptr) noexcept
{
    fm_bptr_assert(ptr);
    _ptr = ptr;
    _count = 1;
}

template <typename T>
void control_block_impl<T>::free_ptr() noexcept
{
    delete static_cast<T*>(_ptr);
}

template <typename T>
control_block_* control_block_impl<T>::create(T* ptr) noexcept
{
    if (ptr)
    {
        auto* __restrict ret = new control_block_impl<T>{ptr};
        return ret;
    }
    else
        return nullptr;
}

} // namespace floormat::detail_borrowed_ptr

namespace floormat {

template<typename T> bptr<T>::bptr(DirectInitT, T* casted_ptr, detail_borrowed_ptr::control_block_* blk) noexcept:
    casted_ptr{casted_ptr}, blk{blk}
{}

//template<typename T> bptr<T>::bptr(NoInitT) noexcept {}

template<typename T>
template<typename... Ts>
requires std::is_constructible_v<T, Ts&&...>
bptr<T>::bptr(InPlaceInitT, Ts&&... args) noexcept:
bptr{ new T{ forward<Ts...>(args...) } }
{
}

template<typename T> bptr<T>::bptr(std::nullptr_t) noexcept: casted_ptr{nullptr}, blk{nullptr} {}
template<typename T> bptr<T>::bptr() noexcept: bptr{nullptr} {}

template<typename T>
bptr<T>::bptr(T* ptr) noexcept:
    casted_ptr{ptr},
    blk{detail_borrowed_ptr::control_block_impl<T>::create(ptr)}
{
    fm_bptr_assert(blk && blk->_count == 1 && ptr && blk->_ptr);
}

template<typename T>
bptr<T>::~bptr() noexcept
{
    if (blk)
        blk->decrement(blk);
}

template<typename T>
template<typename Y>
requires std::is_convertible_v<Y*, T*>
bptr<T>::bptr(const bptr<Y>& other) noexcept:
    casted_ptr{other.casted_ptr}, blk{other.blk}
{
    if (blk)
    {
        ++blk->_count;
        fm_bptr_assert(blk->_count > 1);
    }
}

template<typename T>
template<typename Y>
requires std::is_convertible_v<Y*, T*>
bptr<T>::bptr(bptr<Y>&& other) noexcept:
    casted_ptr{other.casted_ptr}, blk{other.blk}
{
    other.casted_ptr = nullptr;
    other.blk = nullptr;
}

template<typename T>
void bptr<T>::reset() noexcept
{
    if (blk)
    {
        fm_bptr_assert(casted_ptr);
        blk->decrement(blk);
        casted_ptr = nullptr;
        blk = nullptr;
    }
}

template<typename T>
template<bool MaybeEmpty>
void bptr<T>::destroy() noexcept
{
    if constexpr(!MaybeEmpty)
        fm_assert(blk);
    blk->free_ptr();
    blk->_ptr = nullptr;
    casted_ptr = nullptr;
}

template<typename T> bptr<T>& bptr<T>::operator=(std::nullptr_t) noexcept { reset(); return *this; }

template<typename T>
template<typename Y>
requires std::is_convertible_v<Y*, T*>
bptr<T>& bptr<T>::operator=(const bptr<Y>& other) noexcept
{
    if (blk != other.blk)
    {
        CORRADE_ASSUME(this != &other); // todo! see if helps
        if (blk)
            blk->decrement(blk);
        casted_ptr = other.casted_ptr;
        blk = other.blk;
        ++blk->_count;
    }
    return *this;
}

template<typename T>
template<typename Y>
requires std::is_convertible_v<Y*, T*>
bptr<T>& bptr<T>::operator=(bptr<Y>&& other) noexcept
{
    blk->decrement(blk);
    casted_ptr = other.casted_ptr;
    blk = other.blk;
    other.casted_ptr = nullptr;
    other.blk = nullptr;
    return *this;
}

template<typename T> T* bptr<T>::get() const noexcept
{
    if (blk && blk->_ptr)
    {
        fm_bptr_assert(casted_ptr);
        return casted_ptr;
    }
    else
        return nullptr;
}
template<typename T> T* bptr<T>::operator->() const noexcept
{
    auto* ret = get();
    fm_bptr_assert(ret);
    return ret;
}

template<typename T> T& bptr<T>::operator*() const noexcept { return *operator->(); }
template<typename T> bool operator==(const bptr<T>& a, const bptr<T>& b) noexcept { return a.blk == b.blk; }
template<typename T> bptr<T>::operator bool() const noexcept { return get(); }

template<typename T>
void bptr<T>::swap(bptr& other) noexcept
{
    using floormat::swap;
    swap(casted_ptr, other.casted_ptr);
    swap(blk, other.blk);
}

template<typename T> uint32_t bptr<T>::use_count() const noexcept
{
    if (blk) [[likely]]
        return blk->_count;
    else
        return 0;
}

} // namespace floormat

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
