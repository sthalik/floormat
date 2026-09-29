#pragma once
#include "weak-borrowed-ptr.hpp"
#include "borrowed-ptr.inl"

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace floormat {

template<typename T, typename P>
auto basic_weak_bptr<T, P>::_copy(block* ptr) noexcept -> block*
{
    if (ptr && ptr->get())
    {
        ptr->weak_add_ref();
        return ptr;
    }
    else
        return nullptr;
}

template<typename T, typename P>
basic_weak_bptr<T, P>& basic_weak_bptr<T, P>::_copy_assign(block* other) noexcept
{
    if (blk != other)
    {
        auto* old = blk;
        blk = _copy(other);
        block::weak_release(old);
    }
    return *this;
}

template<typename T, typename P>
basic_weak_bptr<T, P>& basic_weak_bptr<T, P>::_move_assign(block*& other) noexcept
{
    auto* old = blk;
    blk = nullptr;
    block::weak_release(old);
    blk = other;
    other = nullptr;
    return *this;
}

template<typename T, typename P> basic_weak_bptr<T, P>::basic_weak_bptr(std::nullptr_t) noexcept: blk{nullptr} {}

template<typename T, typename P>
basic_weak_bptr<T, P>& basic_weak_bptr<T, P>::operator=(std::nullptr_t) noexcept
{
    reset();
    return *this;
}

template<typename T, typename P> basic_weak_bptr<T, P>::basic_weak_bptr() noexcept: basic_weak_bptr{nullptr} {}

template<typename T, typename P> basic_weak_bptr<T, P>::~basic_weak_bptr() noexcept
{
    block::weak_release(blk);
}

template<typename T, typename P> template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr<T, P>::basic_weak_bptr(const basic_bptr<Y, P>& ptr) noexcept: blk{_copy(ptr.blk)} {}
template<typename T, typename P> template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr<T, P>::basic_weak_bptr(const basic_weak_bptr<Y, P>& ptr) noexcept: blk{_copy(ptr.blk)} {}
template<typename T, typename P> basic_weak_bptr<T, P>::basic_weak_bptr(const basic_weak_bptr& ptr) noexcept: blk{_copy(ptr.blk)} {}

template<typename T, typename P> template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr<T, P>& basic_weak_bptr<T, P>::operator=(const basic_bptr<Y, P>& ptr) noexcept { return _copy_assign(ptr.blk); }
template<typename T, typename P> template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr<T, P>& basic_weak_bptr<T, P>::operator=(const basic_weak_bptr<Y, P>& ptr) noexcept { return _copy_assign(ptr.blk); }
template<typename T, typename P> basic_weak_bptr<T, P>& basic_weak_bptr<T, P>::operator=(const basic_weak_bptr& ptr) noexcept { return _copy_assign(ptr.blk); }

template<typename T, typename P>
template<detail_bptr::DerivedFrom<T> Y>
basic_weak_bptr<T, P>::basic_weak_bptr(basic_weak_bptr<Y, P>&& ptr) noexcept: blk{ptr.blk}
{ ptr.blk = nullptr; }

template<typename T, typename P> basic_weak_bptr<T, P>::basic_weak_bptr(basic_weak_bptr&& ptr) noexcept: blk{ptr.blk}
{ ptr.blk = nullptr; }

template<typename T, typename P>
template<detail_bptr::DerivedFrom<T> Y>
basic_weak_bptr<T, P>& basic_weak_bptr<T, P>::operator=(basic_weak_bptr<Y, P>&& ptr) noexcept
{ return _move_assign(ptr.blk); }

template<typename T, typename P> basic_weak_bptr<T, P>& basic_weak_bptr<T, P>::operator=(basic_weak_bptr&& ptr) noexcept { return _move_assign(ptr.blk); }

template<typename T, typename P> void basic_weak_bptr<T, P>::reset() noexcept
{
    auto* old = blk;
    blk = nullptr;
    block::weak_release(old);
}

template<typename T, typename P> void basic_weak_bptr<T, P>::swap(basic_weak_bptr& other) noexcept
{ floormat::swap(blk, other.blk); }

template<typename T, typename P> auto basic_weak_bptr<T, P>::use_count() const noexcept -> count_type
{
    if (blk && blk->get())
        return blk->use_count();
    else
        return 0;
}

template<typename T, typename P> bool basic_weak_bptr<T, P>::expired() const noexcept { return use_count() == 0; }

template<typename T, typename P>
basic_bptr<T, P> basic_weak_bptr<T, P>::lock() const noexcept
{
    if (blk && blk->add_ref_lock())
    {
        basic_bptr<T, P> ret{nullptr};
        ret.blk = blk;
        return ret;
    }
    else
        return basic_bptr<T, P>{nullptr};
}

template<typename T, typename P> bool basic_weak_bptr<T, P>::operator==(const basic_weak_bptr<const T, P>& other) const noexcept
{
    return (blk ? blk->get() : nullptr) == (other.blk ? other.blk->get() : nullptr);
}
template<typename T, typename P> bool basic_weak_bptr<T, P>::operator==(const basic_weak_bptr<T, P>& other) const noexcept requires (!std::is_const_v<T>)
{
    return (blk ? blk->get() : nullptr) == (other.blk ? other.blk->get() : nullptr);
}

} // namespace floormat

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
