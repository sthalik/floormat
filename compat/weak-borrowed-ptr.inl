#pragma once
#include "weak-borrowed-ptr.hpp"

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace floormat {

template<typename T> detail_bptr::control_block* weak_bptr<T>::_copy(detail_bptr::control_block* ptr)
{
    if (ptr && ptr->_ptr)
    {
        ++ptr->_soft_count;
        return ptr;
    }
    else
        return nullptr;
}

template<typename T>
weak_bptr<T>& weak_bptr<T>::_copy_assign(detail_bptr::control_block* other) noexcept
{
    if (blk != other)
    {
        detail_bptr::control_block::weak_decrement(blk);

        if (other && other->_ptr)
        {
            ++other->_soft_count;
            blk = other;
        }
        else
            blk = nullptr;
    }
    return *this;
}

template<typename T>
weak_bptr<T>& weak_bptr<T>::_move_assign(detail_bptr::control_block*& other) noexcept
{
    detail_bptr::control_block::weak_decrement(blk);
    blk = other;
    other = nullptr;
    return *this;
}

template<typename T> weak_bptr<T>::weak_bptr(std::nullptr_t) noexcept: blk{nullptr} {}

template<typename T>
weak_bptr<T>& weak_bptr<T>::operator=(std::nullptr_t) noexcept
{
    detail_bptr::control_block::weak_decrement(blk);
    return *this;
}

template<typename T> weak_bptr<T>::weak_bptr() noexcept: weak_bptr{nullptr} {}

template<typename T> weak_bptr<T>::~weak_bptr() noexcept
{
    detail_bptr::control_block::weak_decrement(blk);
}

template<typename T> template<detail_bptr::DerivedFrom<T> Y> weak_bptr<T>::weak_bptr(const bptr<Y>& ptr) noexcept: blk{_copy(ptr.blk)} {}
template<typename T> template<detail_bptr::DerivedFrom<T> Y> weak_bptr<T>::weak_bptr(const weak_bptr<Y>& ptr) noexcept: blk{_copy(ptr.blk)} {}
template<typename T> weak_bptr<T>::weak_bptr(const weak_bptr& ptr) noexcept: blk{_copy(ptr.blk)} {}

template<typename T> template<detail_bptr::DerivedFrom<T> Y> weak_bptr<T>& weak_bptr<T>::operator=(const bptr<Y>& ptr) noexcept { return _copy_assign(ptr.blk); }
template<typename T> template<detail_bptr::DerivedFrom<T> Y> weak_bptr<T>& weak_bptr<T>::operator=(const weak_bptr<Y>& ptr) noexcept { return _copy_assign(ptr.blk); }
template<typename T> weak_bptr<T>& weak_bptr<T>::operator=(const weak_bptr& ptr) noexcept { return _copy_assign(ptr.blk); }

template<typename T>
template<detail_bptr::DerivedFrom<T> Y>
weak_bptr<T>::weak_bptr(weak_bptr<Y>&& ptr) noexcept: blk{ptr.blk}
{ ptr.blk = nullptr; }

template<typename T> weak_bptr<T>::weak_bptr(weak_bptr&& ptr) noexcept: blk{ptr.blk}
{ ptr.blk = nullptr; }

template<typename T>
template<detail_bptr::DerivedFrom<T> Y>
weak_bptr<T>& weak_bptr<T>::operator=(weak_bptr<Y>&& ptr) noexcept
{ return _move_assign(ptr.blk); }

template<typename T> weak_bptr<T>& weak_bptr<T>::operator=(weak_bptr&& ptr) noexcept { return _move_assign(ptr.blk); }

template<typename T> void weak_bptr<T>::reset() noexcept
{ if (blk) detail_bptr::control_block::weak_decrement(blk); }

template<typename T> void weak_bptr<T>::swap(weak_bptr& other) noexcept
{ floormat::swap(blk, other.blk); }

template<typename T> uint32_t weak_bptr<T>::use_count() const noexcept
{
    if (blk && blk->_ptr)
        return blk->_hard_count;
    else
        return 0;
}

template<typename T> bool weak_bptr<T>::expired() const noexcept { return use_count() == 0; }

template<typename T>
bptr<T> weak_bptr<T>::lock() const noexcept
{
    if (blk && blk->_ptr)
    {
        fm_bptr_assert(blk->_hard_count > 0);
        ++blk->_soft_count;
        ++blk->_hard_count;
        bptr<T> ret{nullptr};
        ret.blk = blk;
        return ret;
    }
    else
        return bptr<T>{nullptr};
}

template<typename T> bool weak_bptr<T>::operator==(const weak_bptr<const T>& other) const noexcept
{
    return lock().get() == other.lock().get();
}

} // namespace floormat

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
