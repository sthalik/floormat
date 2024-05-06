#pragma once
namespace floormat {

template<typename T> class bptr;

template<typename T> bool operator==(const bptr<T>& a, const bptr<T>& b) noexcept;
template<typename To, typename From> bptr<To> static_pointer_cast(const bptr<From>& p) noexcept;

template<typename T> bptr(T* ptr) -> bptr<T>;

} // namespace floormat
