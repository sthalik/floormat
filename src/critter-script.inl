#pragma once
#include "critter-script.hpp"
#include "compat/assert.hpp"

namespace floormat {

template <typename T> script_wrapper<T>::script_wrapper(T* ptr): ptr{ptr} { fm_assert(ptr); }

template <typename T>
script_wrapper<T>::~script_wrapper() noexcept
{
    ptr->delete_self();
#ifndef FM_NO_DEBUG
    ptr = nullptr;
#endif
}

template <typename T> const T& script_wrapper<T>::operator*() const noexcept { return *ptr; }
template <typename T> T& script_wrapper<T>::operator*() noexcept { return *ptr; }
template <typename T> const T* script_wrapper<T>::operator->() const noexcept { return ptr; }
template <typename T> T* script_wrapper<T>::operator->() noexcept { return ptr; }

} // namespace floormat
