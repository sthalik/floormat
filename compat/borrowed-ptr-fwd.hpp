#pragma once

namespace floormat {

#define FM_BPTR_DEBUG
//#define FM_NO_WEAK_BPTR

struct bptr_base;

template<typename T> class bptr;
template<typename T> class weak_bptr;

template<typename T> bptr(T* ptr) -> bptr<T>;

#ifndef FM_NO_WEAK_BPTR
template<typename T> weak_bptr(const bptr<T>& ptr) -> weak_bptr<T>;
#endif

} // namespace floormat
