#pragma once

namespace floormat {

template<typename T> T& non_const(const T& value) { return const_cast<T&>(value); }
template<typename T> T& non_const(T& value) = delete;
template<typename T> T& non_const(T&&) = delete;
template<typename T> T& non_const(const T&& value) = delete;

template<typename T> T& non_const_(const T& value) { return const_cast<T&>(value); }
template<typename T> T& non_const_(T& value) { return value; }
template<typename T> T& non_const_(T&& value) { return static_cast<T&>(value); }
template<typename T> T& non_const_(const T&& value) { return static_cast<T&>(const_cast<T&&>(value)); }

} // namespace floormat
