#pragma once
#include "borrowed-ptr.hpp"

namespace floormat {

template<typename T>
class weak_bptr final
{
    detail_bptr::control_block* blk;

    static detail_bptr::control_block* _copy(detail_bptr::control_block* ptr);
    weak_bptr& _copy_assign(detail_bptr::control_block* other) noexcept;
    weak_bptr& _move_assign(detail_bptr::control_block*& other) noexcept;

public:
    weak_bptr(std::nullptr_t) noexcept;
    weak_bptr& operator=(std::nullptr_t) noexcept;
    weak_bptr() noexcept;
    ~weak_bptr() noexcept;

    template<detail_bptr::DerivedFrom<T> Y> weak_bptr(const bptr<Y>& ptr) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> weak_bptr(const weak_bptr<Y>& ptr) noexcept;
    weak_bptr(const weak_bptr& ptr) noexcept;

    template<detail_bptr::DerivedFrom<T> Y> weak_bptr& operator=(const bptr<Y>& ptr) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> weak_bptr& operator=(const weak_bptr<Y>& ptr) noexcept;
    weak_bptr& operator=(const weak_bptr& ptr) noexcept;

    template<detail_bptr::DerivedFrom<T> Y> weak_bptr(weak_bptr<Y>&& ptr) noexcept;
    weak_bptr(weak_bptr&& ptr) noexcept;

    template<detail_bptr::DerivedFrom<T> Y> weak_bptr& operator=(weak_bptr<Y>&& ptr) noexcept;
    weak_bptr& operator=(weak_bptr&& ptr) noexcept;

    void reset() noexcept;
    void swap(weak_bptr& other) noexcept;

    uint32_t use_count() const noexcept;
    bool expired() const noexcept;
    bptr<T> lock() const noexcept;

    bool operator==(const weak_bptr<const T>& other) const noexcept;
};

} // namespace floormat
