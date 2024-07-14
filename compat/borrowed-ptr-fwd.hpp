#pragma once

namespace floormat {

struct bptr_base;

template<typename T>
requires std::is_convertible_v<T*, const bptr_base*>
class bptr;

template<typename T>
requires std::is_convertible_v<T*, const bptr_base*>
bptr(T* ptr) -> bptr<T>;

template<typename T>
requires std::is_convertible_v<T*, const bptr_base*>
bptr(const T* ptr) -> bptr<const T>;

} // namespace floormat
