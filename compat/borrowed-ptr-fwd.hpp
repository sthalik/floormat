#pragma once

namespace floormat {

struct bptr_base;

template<typename T> class bptr;
template<typename T> class weak_bptr;

template<typename T> bptr(T* ptr) -> bptr<T>;
template<typename T> bptr(const T* ptr) -> bptr<const T>;

} // namespace floormat
