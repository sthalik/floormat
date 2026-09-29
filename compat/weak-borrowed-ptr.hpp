#pragma once
#include "borrowed-ptr.hpp"

namespace floormat {

template<typename T, typename Policy>
class basic_weak_bptr final
{
    static_assert(Policy::has_weak);
    using block = detail_bptr::control_block<Policy>;
    using count_type = typename Policy::counter::value_type;

    block* blk;

    static block* _copy(block* ptr) noexcept;
    basic_weak_bptr& _copy_assign(block* other) noexcept;
    basic_weak_bptr& _move_assign(block*& other) noexcept;

public:
    basic_weak_bptr(std::nullptr_t) noexcept;
    basic_weak_bptr& operator=(std::nullptr_t) noexcept;
    basic_weak_bptr() noexcept;
    ~basic_weak_bptr() noexcept;

    template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr(const basic_bptr<Y, Policy>& ptr) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr(const basic_weak_bptr<Y, Policy>& ptr) noexcept;
    basic_weak_bptr(const basic_weak_bptr& ptr) noexcept;

    template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr& operator=(const basic_bptr<Y, Policy>& ptr) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr& operator=(const basic_weak_bptr<Y, Policy>& ptr) noexcept;
    basic_weak_bptr& operator=(const basic_weak_bptr& ptr) noexcept;

    template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr(basic_weak_bptr<Y, Policy>&& ptr) noexcept;
    basic_weak_bptr(basic_weak_bptr&& ptr) noexcept;

    template<detail_bptr::DerivedFrom<T> Y> basic_weak_bptr& operator=(basic_weak_bptr<Y, Policy>&& ptr) noexcept;
    basic_weak_bptr& operator=(basic_weak_bptr&& ptr) noexcept;

    void reset() noexcept;
    void swap(basic_weak_bptr& other) noexcept;

    count_type use_count() const noexcept;
    bool expired() const noexcept;
    basic_bptr<T, Policy> lock() const noexcept;

    bool operator==(const basic_weak_bptr<const T, Policy>& other) const noexcept;
    bool operator==(const basic_weak_bptr<T, Policy>& other) const noexcept requires (!std::is_const_v<T>);

    template<typename U, typename P> friend class basic_weak_bptr;
};

} // namespace floormat
