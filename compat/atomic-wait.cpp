#include "atomic.hpp"
#include "assert.hpp"
#include <cerrno>
#include <climits>

#if defined _WIN32 && !defined FM_NO_PLATFORM_ATOMIC_WAIT
#define FM_WAIT_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined __linux__ && !defined FM_NO_PLATFORM_ATOMIC_WAIT
#define FM_WAIT_FUTEX
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined __FreeBSD__ && !defined FM_NO_PLATFORM_ATOMIC_WAIT
#define FM_WAIT_UMTX
#include <sys/types.h>
#include <sys/umtx.h>
#elif defined __APPLE__ && !defined FM_NO_PLATFORM_ATOMIC_WAIT && defined __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 140400
#define FM_WAIT_OS_SYNC
#include <os/os_sync_wait_on_address.h>
#else
#define FM_WAIT_POOL
#include <condition_variable>
#include <mutex>
#endif

#if defined FM_WAIT_WIN32 || defined FM_WAIT_POOL
#define FM_WAIT_SIZES_1_2 1
#define FM_WAIT_SIZE_8 1
#elif (defined FM_WAIT_UMTX && defined __LP64__) || defined FM_WAIT_OS_SYNC
#define FM_WAIT_SIZES_1_2 0
#define FM_WAIT_SIZE_8 1
#else
#define FM_WAIT_SIZES_1_2 0
#define FM_WAIT_SIZE_8 0
#endif

namespace floormat {

namespace {

[[maybe_unused]] void* addr(const volatile void* p) noexcept { return const_cast<void*>(p); }

#ifdef FM_WAIT_POOL
struct alignas(64) bucket
{
    std::mutex mutex;
    std::condition_variable cond;
};

constexpr uint32_t bucket_count = 64;
bucket buckets[bucket_count];

bucket& bucket_for(const volatile void* p) noexcept
{
    return buckets[(uint64_t)((uintptr_t)p >> 4) * 0x9e3779b97f4a7c15u >> 58];
}
static_assert(bucket_count == 1 << (64 - 58));
#endif

#if 0
// Lets wake() skip the system call when nothing sleeps on an address in p's slot, as libc++ does.
// Off: EventCount::notify() already skips it, and this adds a fence to every notify and every sleep.
struct alignas(64) waiter_count
{
    volatile uint32_t value = 0;
};

waiter_count waiter_counts[64];

volatile uint32_t* waiters_for(const volatile void* p) noexcept
{
    return &waiter_counts[(uint64_t)((uintptr_t)p >> 4) * 0x9e3779b97f4a7c15u >> 58].value;
}
#endif

template<typename T> void wake(const volatile T* p, int count) noexcept
{
#if 0
    // Pairs with the fence in atomic_wait(): either the waiter's load sees the caller's store,
    // or this load sees the waiter.
    atomic_thread_fence(memory_order::seq_cst);
    if (!atomic_load(waiters_for(p), memory_order::relaxed))
        return;
#endif
#if defined FM_WAIT_WIN32
    if (count == 1)
        WakeByAddressSingle(addr(p));
    else
        WakeByAddressAll(addr(p));
#elif defined FM_WAIT_FUTEX
    long ret = syscall(SYS_futex, addr(p), FUTEX_WAKE_PRIVATE, count);
    (void)ret;
    fm_debug3_assert(ret >= 0);
#elif defined FM_WAIT_UMTX
    // UMTX_OP_WAIT sleeps on the shared queue, UMTX_OP_WAIT_UINT_PRIVATE on the private one.
    int ret = _umtx_op(addr(p), sizeof(T) == 4 ? UMTX_OP_WAKE_PRIVATE : UMTX_OP_WAKE, (u_long)count, nullptr, nullptr);
    (void)ret;
    fm_debug3_assert(ret == 0);
#elif defined FM_WAIT_OS_SYNC
    int ret = count == 1 ? os_sync_wake_by_address_any(addr(p), sizeof(T), OS_SYNC_WAKE_BY_ADDRESS_NONE)
                         : os_sync_wake_by_address_all(addr(p), sizeof(T), OS_SYNC_WAKE_BY_ADDRESS_NONE);
    (void)ret;
    fm_debug3_assert(ret == 0 || errno == ENOENT);
#else
    (void)count;
    auto& b = bucket_for(p);
    // A waiter loads *p under the mutex, so this orders the caller's store before that load.
    b.mutex.lock();
    b.mutex.unlock();
    b.cond.notify_all();
#endif
}

} // namespace

template<atomic_wait_type T> void atomic_wait(const volatile T* p, std::type_identity_t<T> old, memory_order o) noexcept
{
#if 0
    if (atomic_load(p, o) != old)
        return;
    volatile uint32_t* waiters = waiters_for(p);
    atomic_fetch_add(waiters, 1u, memory_order::relaxed);
    atomic_thread_fence(memory_order::seq_cst);
#endif
    while (atomic_load(p, o) == old)
    {
#if defined FM_WAIT_WIN32
        BOOL ret = WaitOnAddress(const_cast<volatile T*>(p), &old, sizeof(T), INFINITE);
        (void)ret;
        fm_debug3_assert(ret);
#elif defined FM_WAIT_FUTEX
        long ret = syscall(SYS_futex, addr(p), FUTEX_WAIT_PRIVATE, (long)(uint32_t)old, nullptr);
        (void)ret;
        fm_debug3_assert(ret == 0 || errno == EAGAIN || errno == EINTR);
#elif defined FM_WAIT_UMTX
        // The kernel zero-extends the u_int it reads, so a sign-extended `old` would never match.
        int ret;
        if constexpr (sizeof(T) == 4)
            ret = _umtx_op(addr(p), UMTX_OP_WAIT_UINT_PRIVATE, (u_long)(uint32_t)old, nullptr, nullptr);
        else
            ret = _umtx_op(addr(p), UMTX_OP_WAIT, (u_long)old, nullptr, nullptr);
        (void)ret;
        fm_debug3_assert(ret == 0 || errno == EINTR);
#elif defined FM_WAIT_OS_SYNC
        // EFAULT can be transient, under memory pressure.
        int ret = os_sync_wait_on_address(addr(p), (uint64_t)(std::make_unsigned_t<T>)old, sizeof(T), OS_SYNC_WAIT_ON_ADDRESS_NONE);
        (void)ret;
        fm_debug3_assert(ret >= 0 || errno == EINTR || errno == EFAULT);
#else
        auto& b = bucket_for(p);
        std::unique_lock lk{b.mutex};
        if (atomic_load(p, memory_order::relaxed) == old)
            b.cond.wait(lk);
#endif
    }
#if 0
    atomic_fetch_sub(waiters, 1u, memory_order::release);
#endif
}

template<atomic_wait_type T> void atomic_notify_one(const volatile T* p) noexcept { wake(p, 1); }
template<atomic_wait_type T> void atomic_notify_all(const volatile T* p) noexcept { wake(p, INT_MAX); }

#define FM_ATOMIC_WAIT_INSTANTIATE(T)                                                               \
    template void atomic_wait<T>(const volatile T*, T, memory_order) noexcept;                      \
    template void atomic_notify_one<T>(const volatile T*) noexcept;                                 \
    template void atomic_notify_all<T>(const volatile T*) noexcept;

#if FM_WAIT_SIZES_1_2
FM_ATOMIC_WAIT_INSTANTIATE(signed char)
FM_ATOMIC_WAIT_INSTANTIATE(unsigned char)
FM_ATOMIC_WAIT_INSTANTIATE(short)
FM_ATOMIC_WAIT_INSTANTIATE(unsigned short)
#endif
FM_ATOMIC_WAIT_INSTANTIATE(int)
FM_ATOMIC_WAIT_INSTANTIATE(unsigned)
#if FM_WAIT_SIZE_8 || __SIZEOF_LONG__ == 4
FM_ATOMIC_WAIT_INSTANTIATE(long)
FM_ATOMIC_WAIT_INSTANTIATE(unsigned long)
#endif
#if FM_WAIT_SIZE_8
FM_ATOMIC_WAIT_INSTANTIATE(long long)
FM_ATOMIC_WAIT_INSTANTIATE(unsigned long long)
#endif

#undef FM_ATOMIC_WAIT_INSTANTIATE

static_assert(atomic_wait_type<short> == FM_WAIT_SIZES_1_2);
static_assert(atomic_wait_type<long long> == FM_WAIT_SIZE_8);
static_assert(atomic_wait_type<long> == (FM_WAIT_SIZE_8 || sizeof(long) == 4));

} // namespace floormat
