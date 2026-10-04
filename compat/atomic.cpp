#include "atomic.hpp"
#include "defs.hpp"
#include <type_traits>

#if defined _MSC_VER && !defined __clang__
#define FM_ATOMIC_MSVC 1
#include <intrin.h>
#if !(defined _M_IX86 || defined _M_X64 || defined _M_ARM64)
#error "unsupported MSVC target"
#endif
#else
#define FM_ATOMIC_MSVC 0
#endif

#if defined __GNUC__ && !defined __clang__
// Same -Wattributes case as src/spritebatch.cpp.
#pragma GCC diagnostic ignored "-Wattributes"
// TSan doesn't model fences. borrowed-ptr-atomic.hpp avoids relying on one under TSan.
#pragma GCC diagnostic ignored "-Wtsan"
#endif

namespace floormat {

namespace {

#if !FM_ATOMIC_MSVC

enum class op : uint8_t { load, store, rmw, };

// An order the operation can't take becomes seq_cst, as GCC does itself.
template<op K, int N>
constexpr inline int valid_order = (K == op::load  && (N == __ATOMIC_RELEASE || N == __ATOMIC_ACQ_REL)) ||
                                   (K == op::store && (N == __ATOMIC_ACQUIRE || N == __ATOMIC_ACQ_REL))
                                   ? __ATOMIC_SEQ_CST : N;

template<int N> using order_c = std::integral_constant<int, N>;

// GCC treats an order that isn't a constant as seq_cst, so each case passes one.
template<op K, typename F>
[[fm_always_inline]] inline decltype(auto) with_order(memory_order o, F&& f) noexcept
{
    switch (o)
    {
    case memory_order::relaxed: return f(order_c<valid_order<K, __ATOMIC_RELAXED>>{});
    case memory_order::acquire: return f(order_c<valid_order<K, __ATOMIC_ACQUIRE>>{});
    case memory_order::release: return f(order_c<valid_order<K, __ATOMIC_RELEASE>>{});
    case memory_order::acq_rel: return f(order_c<valid_order<K, __ATOMIC_ACQ_REL>>{});
    case memory_order::seq_cst: break;
    }
    return f(order_c<__ATOMIC_SEQ_CST>{});
}

// failure order of a compare-exchange can't be release or acq_rel
constexpr int failure_order(int o) noexcept
{
    switch (o)
    {
    case __ATOMIC_RELEASE: return __ATOMIC_RELAXED;
    case __ATOMIC_ACQ_REL: return __ATOMIC_ACQUIRE;
    default: return o;
    }
}

// i386 SysV only aligns 64-bit integers to 4 bytes. Tell the compiler the
// operand is naturally aligned so that it inlines cmpxchg8b instead of
// calling libatomic. Callers must keep 64-bit atomics 8-byte aligned.
typedef int64_t aligned_int64_t __attribute__((aligned(8)));
typedef uint64_t aligned_uint64_t __attribute__((aligned(8)));
template<typename T> struct aligned_ { using type = T; };
template<> struct aligned_<int64_t> { using type = aligned_int64_t; };
template<> struct aligned_<uint64_t> { using type = aligned_uint64_t; };
template<typename T> using A = typename aligned_<T>::type;

template<typename T> [[fm_always_inline]] inline volatile A<T>* al(volatile T* p) noexcept { return (volatile A<T>*)p; }
template<typename T> [[fm_always_inline]] inline const volatile A<T>* al(const volatile T* p) noexcept { return (const volatile A<T>*)p; }

#define FM_ATOMIC_ORDER(c) decltype(c)::value

#else

#ifdef _M_ARM64
void hw_fence() noexcept { __dmb(_ARM64_BARRIER_ISH); }
#else
void compiler_barrier() noexcept
{
#pragma warning(push)
#pragma warning(disable : 4996) // _ReadWriteBarrier is deprecated
    _ReadWriteBarrier();
#pragma warning(pop)
}
#endif

// Unsuffixed Interlocked intrinsics are full barriers on every target.
template<size_t N> struct ops;

template<> struct ops<1>
{
    using I = char;
    static I load(const volatile I* p) noexcept { return __iso_volatile_load8((const volatile __int8*)p); }
    static void store(volatile I* p, I x) noexcept { __iso_volatile_store8((volatile __int8*)p, x); }
    static I xchg(volatile I* p, I x) noexcept { return _InterlockedExchange8(p, x); }
    static I cas(volatile I* p, I x, I cmp) noexcept { return _InterlockedCompareExchange8(p, x, cmp); }
    static I add(volatile I* p, I x) noexcept { return _InterlockedExchangeAdd8(p, x); }
    static I and_(volatile I* p, I x) noexcept { return _InterlockedAnd8(p, x); }
    static I or_(volatile I* p, I x) noexcept { return _InterlockedOr8(p, x); }
    static I xor_(volatile I* p, I x) noexcept { return _InterlockedXor8(p, x); }
};

template<> struct ops<2>
{
    using I = short;
    static I load(const volatile I* p) noexcept { return __iso_volatile_load16(p); }
    static void store(volatile I* p, I x) noexcept { __iso_volatile_store16(p, x); }
    static I xchg(volatile I* p, I x) noexcept { return _InterlockedExchange16(p, x); }
    static I cas(volatile I* p, I x, I cmp) noexcept { return _InterlockedCompareExchange16(p, x, cmp); }
    static I add(volatile I* p, I x) noexcept { return _InterlockedExchangeAdd16(p, x); }
    static I and_(volatile I* p, I x) noexcept { return _InterlockedAnd16(p, x); }
    static I or_(volatile I* p, I x) noexcept { return _InterlockedOr16(p, x); }
    static I xor_(volatile I* p, I x) noexcept { return _InterlockedXor16(p, x); }
};

template<> struct ops<4>
{
    using I = long;
    static I load(const volatile I* p) noexcept { return __iso_volatile_load32((const volatile int*)p); }
    static void store(volatile I* p, I x) noexcept { __iso_volatile_store32((volatile int*)p, (int)x); }
    static I xchg(volatile I* p, I x) noexcept { return _InterlockedExchange(p, x); }
    static I cas(volatile I* p, I x, I cmp) noexcept { return _InterlockedCompareExchange(p, x, cmp); }
    static I add(volatile I* p, I x) noexcept { return _InterlockedExchangeAdd(p, x); }
    static I and_(volatile I* p, I x) noexcept { return _InterlockedAnd(p, x); }
    static I or_(volatile I* p, I x) noexcept { return _InterlockedOr(p, x); }
    static I xor_(volatile I* p, I x) noexcept { return _InterlockedXor(p, x); }
};

#ifdef _M_IX86
// x86-32 only has cmpxchg8b for 64-bit values, so build everything on top of it.
template<> struct ops<8>
{
    using I = __int64;
    static I cas(volatile I* p, I x, I cmp) noexcept { return _InterlockedCompareExchange64(p, x, cmp); }
    template<typename F> static I rmw(volatile I* p, F&& f) noexcept
    {
        I old = cas(p, 0, 0);
        for (;;)
        {
            I prev = cas(p, f(old), old);
            if (prev == old)
                return old;
            old = prev;
        }
    }
    // An aligned 8-byte x87 load is atomic (this is what GCC emits too).
    // cmpxchg8b would write to the line, and intrinsics get split into two
    // 32-bit loads by the optimizer.
    static I load(const volatile I* p) noexcept
    {
        I value;
        __asm {
            mov eax, p
            fild qword ptr [eax]
            fistp qword ptr [value]
        }
        return value;
    }
    static void store(volatile I* p, I x) noexcept { (void)xchg(p, x); }
    static I xchg(volatile I* p, I x) noexcept { return rmw(p, [x](I) { return x; }); }
    static I add(volatile I* p, I x) noexcept { return rmw(p, [x](I v) { return (I)((unsigned __int64)v + (unsigned __int64)x); }); }
    static I and_(volatile I* p, I x) noexcept { return rmw(p, [x](I v) { return v & x; }); }
    static I or_(volatile I* p, I x) noexcept { return rmw(p, [x](I v) { return v | x; }); }
    static I xor_(volatile I* p, I x) noexcept { return rmw(p, [x](I v) { return v ^ x; }); }
};
#else
template<> struct ops<8>
{
    using I = __int64;
    static I load(const volatile I* p) noexcept { return __iso_volatile_load64(p); }
    static void store(volatile I* p, I x) noexcept { __iso_volatile_store64(p, x); }
    static I xchg(volatile I* p, I x) noexcept { return _InterlockedExchange64(p, x); }
    static I cas(volatile I* p, I x, I cmp) noexcept { return _InterlockedCompareExchange64(p, x, cmp); }
    static I add(volatile I* p, I x) noexcept { return _InterlockedExchangeAdd64(p, x); }
    static I and_(volatile I* p, I x) noexcept { return _InterlockedAnd64(p, x); }
    static I or_(volatile I* p, I x) noexcept { return _InterlockedOr64(p, x); }
    static I xor_(volatile I* p, I x) noexcept { return _InterlockedXor64(p, x); }
};
#endif

template<typename T> using Ops = ops<sizeof(T)>;
template<typename T> using I = typename Ops<T>::I;

template<typename T> volatile I<T>* ptr(volatile T* p) noexcept { return reinterpret_cast<volatile I<T>*>(p); }
template<typename T> const volatile I<T>* ptr(const volatile T* p) noexcept { return reinterpret_cast<const volatile I<T>*>(p); }
template<typename T> I<T> to_int(T x) noexcept { return (I<T>)x; }

#endif

} // namespace

template<atomic_type T> [[fm_always_inline]] T atomic_load(const volatile T* p, memory_order o) noexcept
{
#if !FM_ATOMIC_MSVC
    return with_order<op::load>(o, [p](auto c) __attribute__((always_inline)) {
        return __atomic_load_n(al(p), FM_ATOMIC_ORDER(c));
    });
#else
    T ret = (T)Ops<T>::load(ptr(p));
#ifdef _M_ARM64
    if (o != memory_order::relaxed)
        hw_fence();
#else
    (void)o;
    compiler_barrier();
#endif
    return ret;
#endif
}

template<atomic_type T> [[fm_always_inline]] void atomic_store(volatile T* p, std::type_identity_t<T> x, memory_order o) noexcept
{
#if !FM_ATOMIC_MSVC
    with_order<op::store>(o, [p, x](auto c) __attribute__((always_inline)) {
        __atomic_store_n(al(p), x, FM_ATOMIC_ORDER(c));
    });
#else
    if (o == memory_order::seq_cst)
        (void)Ops<T>::xchg(ptr(p), to_int(x));
    else
    {
#ifdef _M_ARM64
        if (o != memory_order::relaxed)
            hw_fence();
#else
        compiler_barrier();
#endif
        Ops<T>::store(ptr(p), to_int(x));
    }
#endif
}

template<atomic_type T> [[fm_always_inline]] T atomic_exchange(volatile T* p, std::type_identity_t<T> x, memory_order o) noexcept
{
#if !FM_ATOMIC_MSVC
    return with_order<op::rmw>(o, [p, x](auto c) __attribute__((always_inline)) {
        return __atomic_exchange_n(al(p), x, FM_ATOMIC_ORDER(c));
    });
#else
    (void)o;
    return (T)Ops<T>::xchg(ptr(p), to_int(x));
#endif
}

template<atomic_type T> [[fm_always_inline]] bool atomic_compare_exchange(volatile T* p, T& expected, std::type_identity_t<T> desired, memory_order o, bool weak) noexcept
{
#if !FM_ATOMIC_MSVC
    A<T> e = expected;
    bool ret = with_order<op::rmw>(o, [&](auto c) __attribute__((always_inline)) {
        constexpr int success = FM_ATOMIC_ORDER(c), failure = failure_order(success);
        if (weak)
            return __atomic_compare_exchange_n(al(p), &e, desired, true, success, failure);
        else
            return __atomic_compare_exchange_n(al(p), &e, desired, false, success, failure);
    });
    expected = e;
    return ret;
#else
    (void)o; (void)weak;
    auto cmp = to_int(expected);
    auto old = Ops<T>::cas(ptr(p), to_int(desired), cmp);
    if (old == cmp)
        return true;
    expected = (T)old;
    return false;
#endif
}

template<atomic_type T> [[fm_always_inline]] bool atomic_compare_exchange_weak(volatile T* p, T& expected, std::type_identity_t<T> desired, memory_order o) noexcept
{
    return atomic_compare_exchange(p, expected, desired, o, true);
}

#if !FM_ATOMIC_MSVC
#define FM_ATOMIC_FETCH_OP(name, msvc_op, msvc_arg)                                                 \
    template<atomic_type T> [[fm_always_inline]]                                                    \
    T atomic_fetch_##name(volatile T* p, std::type_identity_t<T> x, memory_order o) noexcept        \
    {                                                                                               \
        return with_order<op::rmw>(o, [p, x](auto c) __attribute__((always_inline)) {              \
            return __atomic_fetch_##name(al(p), x, FM_ATOMIC_ORDER(c));                             \
        });                                                                                         \
    }
#else
#define FM_ATOMIC_FETCH_OP(name, msvc_op, msvc_arg)                                                 \
    template<atomic_type T> [[fm_always_inline]]                                                    \
    T atomic_fetch_##name(volatile T* p, std::type_identity_t<T> x, memory_order o) noexcept        \
    {                                                                                               \
        (void)o;                                                                                    \
        return (T)Ops<T>::msvc_op(ptr(p), msvc_arg);                                                \
    }
#endif

FM_ATOMIC_FETCH_OP(add, add,  to_int(x))
FM_ATOMIC_FETCH_OP(sub, add,  to_int(T(std::make_unsigned_t<T>(0) - std::make_unsigned_t<T>(x))))
FM_ATOMIC_FETCH_OP(and, and_, to_int(x))
FM_ATOMIC_FETCH_OP(or,  or_,  to_int(x))
FM_ATOMIC_FETCH_OP(xor, xor_, to_int(x))

#undef FM_ATOMIC_FETCH_OP

[[fm_always_inline]] void atomic_thread_fence(memory_order o) noexcept
{
#if !FM_ATOMIC_MSVC
    with_order<op::rmw>(o, [](auto c) __attribute__((always_inline)) {
        __atomic_thread_fence(FM_ATOMIC_ORDER(c));
    });
#else
    if (o == memory_order::relaxed)
        return;
#ifdef _M_ARM64
    hw_fence();
#else
    if (o == memory_order::seq_cst)
    {
        volatile long guard = 0;
        (void)_InterlockedOr(&guard, 0);
    }
    else
        compiler_barrier();
#endif
#endif
}

[[fm_always_inline]] void cpu_relax() noexcept
{
#if FM_ATOMIC_MSVC
#ifdef _M_ARM64
    __yield();
#else
    _mm_pause();
#endif
#elif defined __x86_64__ || defined __i386__
    __builtin_ia32_pause();
#elif defined __aarch64__
    asm volatile("yield");
#elif defined __riscv
    asm volatile("pause");
#elif defined __powerpc__ || defined __powerpc64__
    asm volatile("or 27,27,27");
#else
    asm volatile("" ::: "memory");
#endif
}

#define FM_ATOMIC_INSTANTIATE(T)                                                                            \
    template T atomic_load<T>(const volatile T*, memory_order) noexcept;                                    \
    template void atomic_store<T>(volatile T*, T, memory_order) noexcept;                                   \
    template T atomic_exchange<T>(volatile T*, T, memory_order) noexcept;                                   \
    template bool atomic_compare_exchange<T>(volatile T*, T&, T, memory_order, bool) noexcept;              \
    template bool atomic_compare_exchange_weak<T>(volatile T*, T&, T, memory_order) noexcept;               \
    template T atomic_fetch_add<T>(volatile T*, T, memory_order) noexcept;                                  \
    template T atomic_fetch_sub<T>(volatile T*, T, memory_order) noexcept;                                  \
    template T atomic_fetch_and<T>(volatile T*, T, memory_order) noexcept;                                  \
    template T atomic_fetch_or <T>(volatile T*, T, memory_order) noexcept;                                  \
    template T atomic_fetch_xor<T>(volatile T*, T, memory_order) noexcept;

FM_ATOMIC_INSTANTIATE(int8_t)
FM_ATOMIC_INSTANTIATE(uint8_t)
FM_ATOMIC_INSTANTIATE(int16_t)
FM_ATOMIC_INSTANTIATE(uint16_t)
FM_ATOMIC_INSTANTIATE(int32_t)
FM_ATOMIC_INSTANTIATE(uint32_t)
FM_ATOMIC_INSTANTIATE(int64_t)
FM_ATOMIC_INSTANTIATE(uint64_t)

#undef FM_ATOMIC_INSTANTIATE

} // namespace floormat
