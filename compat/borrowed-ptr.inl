#pragma once
#include "borrowed-ptr.hpp"
#include "assert.hpp"

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace floormat::detail_bptr {

template<typename Policy>
inline bool control_block<Policy>::add_ref_lock() noexcept
{
    if constexpr (counter::concurrent)
    {
        if (!counter::increment_if_nonzero(_hard_count, *this))
            return false;
        stats::copied();
        // Only after the increment: destroy() clears _ptr while hard refs remain.
        if (get())
            return true;
        release(this);
        return false;
    }
    else
    {
        if (!get())
            return false;
        fm_debug3_assert(counter::load(_hard_count) > 0);
        add_ref();
        return true;
    }
}

template<typename Policy>
void control_block<Policy>::release(control_block* b) noexcept
{
    if (!b)
        return;
    stats::released();
    auto c = counter::decrement(b->_hard_count, *b);
    fm_debug3_assert(c != (count_type)-1);
    if (c == 0)
    {
        // Null before dispose so a weak_bptr::lock() from within the destructor
        // sees an expired block instead of resurrecting a dying object.
        if (auto* p = to_ptr(counter::exchange(b->_ptr, to_int(nullptr))))
        {
            stats::object_disposed();
            b->dispose(p);
        }
        if constexpr (Policy::has_weak)
            weak_release(b);
        else
        {
            stats::block_deallocated();
            b->deallocate();
        }
    }
}

template<typename Policy>
void control_block<Policy>::weak_release(control_block* b) noexcept
{
    if (!b)
        return;
    // Another thread can change either count between the two loads.
    if constexpr (!counter::concurrent)
        fm_debug3_assert(counter::load(b->_soft_count) > (counter::load(b->_hard_count) ? 1u : 0u));
    auto c = counter::decrement(b->_soft_count, *b);
    if (c == 0)
    {
        fm_debug3_assert(!b->get());
        stats::block_deallocated();
        b->deallocate();
    }
}

template<typename Policy>
void control_block<Policy>::destroy_object(control_block* b) noexcept
{
    // The destructor can drop the last reference, and an in-place object lives
    // inside the block, so hold one until the destructor returns.
    b->add_ref();
    if (auto* p = to_ptr(counter::exchange(b->_ptr, to_int(nullptr))))
    {
        stats::object_disposed();
        b->dispose(p);
    }
    release(b);
}

} // namespace floormat::detail_bptr

namespace floormat {

template<typename T, typename P> basic_bptr<T, P>::~basic_bptr() noexcept { block::release(blk); }

template<typename T, typename P> basic_bptr<T, P>::basic_bptr(const basic_bptr<std::remove_const_t<T>, P>& ptr) noexcept requires std::is_const_v<T>: basic_bptr{ptr, nullptr} {}
template<typename T, typename P> basic_bptr<T, P>::basic_bptr(basic_bptr<std::remove_const_t<T>, P>&& ptr) noexcept requires std::is_const_v<T>: basic_bptr{move(ptr), nullptr} {}

template<typename T, typename P> basic_bptr<T, P>::basic_bptr(const basic_bptr& other) noexcept: basic_bptr{other, nullptr} {}
template<typename T, typename P> basic_bptr<T, P>::basic_bptr(basic_bptr&& other) noexcept: basic_bptr{move(other), nullptr} {}
template<typename T, typename P> basic_bptr<T, P>& basic_bptr<T, P>::operator=(const basic_bptr& other) noexcept { return _copy_assign(other); }
template<typename T, typename P> basic_bptr<T, P>& basic_bptr<T, P>::operator=(basic_bptr&& other) noexcept { return _move_assign(move(other)); }

template<typename T, typename P>
template<detail_bptr::DerivedFrom<T> Y>
basic_bptr<T, P>::basic_bptr(const basic_bptr<Y, P>& other) noexcept:
    basic_bptr{other, nullptr}
{}

template<typename T, typename P>
template<detail_bptr::DerivedFrom<T> Y>
basic_bptr<T, P>& basic_bptr<T, P>::operator=(const basic_bptr<Y, P>& other) noexcept
{ return _copy_assign(other); }

template<typename T, typename P>
template<detail_bptr::DerivedFrom<T> Y>
basic_bptr<T, P>::basic_bptr(basic_bptr<Y, P>&& other) noexcept:
    basic_bptr{move(other), nullptr}
{}

template<typename T, typename P>
template<detail_bptr::DerivedFrom<T> Y>
basic_bptr<T, P>& basic_bptr<T, P>::operator=(basic_bptr<Y, P>&& other) noexcept
{ return _move_assign(move(other)); }

// Releasing the last reference can destroy the storage holding *this, so every release detaches
// the block first and touches nothing of *this afterwards.
template<typename T, typename P>
void basic_bptr<T, P>::reset() noexcept
{
    auto* old = blk;
    blk = nullptr;
    block::release(old);
}

template<typename T, typename P>
template<detail_bptr::DerivedFrom<T> Y>
void basic_bptr<T, P>::reset(Y* ptr) noexcept
{
    auto* old = blk;
    blk = detail_bptr::block_from_raw<std::remove_const_t<Y>, P>(const_cast<std::remove_const_t<Y>*>(ptr));
    block::release(old);
}

template<typename T, typename P>
void basic_bptr<T, P>::destroy() noexcept
{
    if (blk)
        block::destroy_object(blk);
}

template<typename T, typename P> basic_bptr<T, P>& basic_bptr<T, P>::operator=(std::nullptr_t) noexcept { reset(); return *this; }

template<typename T, typename P>
template<typename Y>
basic_bptr<T, P>::basic_bptr(const basic_bptr<Y, P>& other, std::nullptr_t) noexcept:
    blk{other.blk}
{
    if (blk)
        blk->add_ref();
}

template<typename T, typename P>
template<typename Y>
basic_bptr<T, P>::basic_bptr(basic_bptr<Y, P>&& other, std::nullptr_t) noexcept:
    blk{other.blk}
{
    other.blk = nullptr;
}

template<typename T, typename P>
template<typename Y>
basic_bptr<T, P>& basic_bptr<T, P>::_copy_assign(const basic_bptr<Y, P>& other) noexcept
{
    if (blk != other.blk)
    {
        // Retain other's block before releasing ours: an aliased assignment
        // (a = a->child) can destroy the object holding other when we decrement.
        auto* new_blk = other.blk;
        if (new_blk)
            new_blk->add_ref();
        auto* old = blk;
        blk = new_blk;
        block::release(old);
    }
    return *this;
}

template<typename T, typename P>
template<typename Y>
basic_bptr<T, P>& basic_bptr<T, P>::_move_assign(basic_bptr<Y, P>&& other) noexcept
{
    // Detach other before releasing ours, for self-move and a = move(a->child).
    auto* new_blk = other.blk;
    other.blk = nullptr;
    auto* old = blk;
    blk = new_blk;
    block::release(old);
    return *this;
}

template<typename T, typename P>
T* basic_bptr<T, P>::get() const noexcept
{
    if (blk) [[likely]]
        return static_cast<T*>(blk->get());
    else
        return nullptr;
}

template<typename T, typename P>
T* basic_bptr<T, P>::operator->() const noexcept
{
    auto* ret = get();
    fm_debug3_assert(ret);
    return ret;
}

template<typename T, typename P> T& basic_bptr<T, P>::operator*() const noexcept { return *operator->(); }

template<typename T, typename P> basic_bptr<T, P>::operator bool() const noexcept { return blk && blk->get(); }
template<typename T, typename P> bool basic_bptr<T, P>::has_block() const noexcept { return blk != nullptr; }

template<typename T, typename P> bool basic_bptr<T, P>::operator==(const basic_bptr<const T, P>& other) const noexcept
{
    return (blk ? blk->get() : nullptr) == (other.blk ? other.blk->get() : nullptr);
}
template<typename T, typename P> bool basic_bptr<T, P>::operator==(const basic_bptr<T, P>& other) const noexcept requires (!std::is_const_v<T>) {
    return (blk ? blk->get() : nullptr) == (other.blk ? other.blk->get() : nullptr);
}

template<typename T, typename P> bool basic_bptr<T, P>::operator==(const std::nullptr_t&) const noexcept { return !blk || !blk->get(); }

template<typename T, typename P> void basic_bptr<T, P>::swap(basic_bptr& other) noexcept { floormat::swap(blk, other.blk); }

template<typename T, typename P>
auto basic_bptr<T, P>::use_count() const noexcept -> count_type
{
    if (blk && blk->get()) [[likely]]
        return blk->use_count();
    else
        return 0;
}

template<typename To, typename From, typename P>
requires detail_bptr::StaticCastable<From, To>
basic_bptr<To, P> static_pointer_cast(basic_bptr<From, P>&& p) noexcept
{
    if (p.blk && p.blk->get()) [[likely]]
    {
        basic_bptr<To, P> ret{nullptr};
        ret.blk = p.blk;
        p.blk = nullptr;
        return ret;
    }
    return basic_bptr<To, P>{nullptr};
}

template<typename To, typename From, typename P>
requires detail_bptr::StaticCastable<From, To>
basic_bptr<To, P> static_pointer_cast(const basic_bptr<From, P>& p) noexcept
{
    if (p.blk && p.blk->get()) [[likely]]
    {
        basic_bptr<To, P> ret{nullptr};
        p.blk->add_ref();
        ret.blk = p.blk;
        return ret;
    }
    return basic_bptr<To, P>{nullptr};
}

} // namespace floormat

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
