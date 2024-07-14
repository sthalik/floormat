#pragma once
namespace floormat {

template<typename T> class bptr;
template<typename To, typename From> bptr<To> static_pointer_cast(const bptr<From>& p) noexcept;
template<typename T> bptr(T* ptr) -> bptr<T>;
template<typename T> bptr(const T* ptr) -> bptr<const T>;

} // namespace floormat
