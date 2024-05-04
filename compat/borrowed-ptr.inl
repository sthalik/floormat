#pragma once
#include "compat/assert.hpp"
#include "borrowed-ptr.hpp"

namespace floormat::detail_borrowed_ptr {

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

struct control_block_
{
    explicit control_block_(void* ptr) noexcept;
    ~control_block_() noexcept;
    void incr() noexcept;
    void decr() noexcept;
    uint32_t count() const noexcept;

    void* _ptr; // todo maybe add directly embeddable objects?
    uint32_t _count;

private:
    virtual void free() noexcept = 0;
};

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

template<typename T>
struct control_block final: control_block_
{
    void free() noexcept override;
    [[nodiscard]] static control_block_* create(T* ptr) noexcept;

protected:
    explicit control_block(T* ptr) noexcept;
};

template <typename T>
control_block<T>::control_block(T* ptr) noexcept:
    control_block_{ptr}
{
    fm_debug_assert(ptr);
}

template <typename T>
void control_block<T>::free() noexcept
{
    delete static_cast<T*>(_ptr);
}

template <typename T>
control_block_* control_block<T>::create(T* ptr) noexcept
{
    return ptr ? new control_block<T>{ptr} : nullptr;
}

} // namespace floormat::detail_borrowed_ptr

namespace floormat {

template<typename T>
template<typename... Ts>
requires std::is_constructible_v<T, Ts&&...>
bptr<T>::bptr(InPlaceInitT, Ts&&... args) noexcept:
bptr{ new T{ forward<Ts...>(args...) } }
{
}

template<typename T>
bptr<T>::bptr(T* ptr) noexcept:
    ptr{ptr},
    blk{detail_borrowed_ptr::control_block<T>::create(ptr)}
{
}

template<typename T>
bptr<T>::~bptr() noexcept
{
    if (blk)
        blk->decr();
    //blk = reinterpret_cast<T*>(-1);
    //blk = nullptr;
}

template<typename T>
template<typename Y>
requires std::is_convertible_v<Y*, T*>
bptr<T>::bptr(const bptr<Y>& other) noexcept:
    ptr{other.ptr}, blk{other.blk}
{
    static_assert(std::is_convertible_v<Y*, T*>);
    if (blk)
        blk->incr();
    else
        fm_debug_assert(!ptr);
}

template<typename T>
template<typename Y>
requires std::is_convertible_v<Y*, T*>
bptr<T>::bptr(bptr<Y>&& other) noexcept: ptr{other.ptr}, blk{other.blk}
{
    other.ptr = nullptr;
    other.blk = nullptr;
}

template<typename T>
template<typename Y>
requires std::is_convertible_v<Y*, T*>
bptr<T>& bptr<T>::operator=(const bptr<Y>& other) noexcept
{
    static_assert(std::is_convertible_v<Y*, T*>);
    auto* const newblk = other.blk;
    if (blk != newblk)
    {
        CORRADE_ASSUME(this != &other);
        ptr = other.ptr;
        if (blk)
            blk->decr();
        blk = newblk;
        if (newblk)
            newblk->incr();
    }
    return *this;
}

template<typename T>
template<typename Y>
requires std::is_convertible_v<Y*, T*>
bptr<T>& bptr<T>::operator=(bptr<Y>&& other) noexcept
{
    ptr = other.ptr;
    blk = other.blk;
    other.ptr = nullptr;
    other.blk = nullptr;
    return *this;
}

template<typename T>
template<typename U>
requires detail_borrowed_ptr::StaticCastable<T, U>
bptr<U> bptr<T>::static_pointer_cast() noexcept
{
    auto ret = bptr<T>{NoInit};
    ret.blk = blk;
    if (ret.blk) [[likely]]
        ret.blk->incr();
    ret.ptr = static_cast<T*>(ptr);
    return ret;
}

} // namespace floormat
