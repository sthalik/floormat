#pragma once

namespace floormat {

struct bptr_base;

struct non_atomic_refcount;
struct atomic_refcount;
struct thread_checked_refcount;

template<typename T, typename Policy> class basic_bptr;
template<typename T, typename Policy> class basic_weak_bptr;

template<typename T> using bptr = basic_bptr<T, non_atomic_refcount>;
template<typename T> using weak_bptr = basic_weak_bptr<T, non_atomic_refcount>;

template<typename T, typename Policy> basic_weak_bptr(const basic_bptr<T, Policy>& ptr) -> basic_weak_bptr<T, Policy>;

} // namespace floormat
