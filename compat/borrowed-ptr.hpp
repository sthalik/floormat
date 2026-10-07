#pragma once
#include "borrowed-ptr-fwd.hpp"
#include "borrowed-ptr-policy.hpp"
#include "assert.hpp"
#include "defs.hpp"
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace floormat {
struct bptr_base
{
    virtual ~bptr_base() noexcept;
    bptr_base() noexcept;
    bptr_base(const bptr_base&) noexcept;
    bptr_base(bptr_base&&) noexcept;
    bptr_base& operator=(const bptr_base&) noexcept;
    bptr_base& operator=(bptr_base&&) noexcept;
};

// bptr{InPlace} puts the control block right in front of the object, so bptr{raw} finds it.
// Must be the first polymorphic base. test/bptr-intrusive.cpp lists what can't work.
template<typename Policy>
struct intrusive_bptr_base : bptr_base
{
    using intrusive_policy = Policy;

    void* operator new(size_t) = delete;
    void* operator new(size_t, std::align_val_t) = delete;
    void* operator new[](size_t) = delete;
    void* operator new[](size_t, std::align_val_t) = delete;
    // The deleted overloads would hide the global placement new from Corrade's containers.
    void* operator new(size_t, void* p) noexcept { return p; }
    void operator delete(void*, void*) noexcept {}
    // A deleted one makes the virtual destructor ill-formed.
    void operator delete(void*) noexcept { fm_abort("delete on an intrusive bptr object"); }
};
} // namespace floormat

namespace floormat::detail_bptr {

template<typename Policy>
struct control_block : Policy::counter::block_state
{
    using counter = typename Policy::counter;
    using count_type = typename counter::value_type;
    using stats = typename Policy::stats;
    template<typename X> using cell = typename counter::template cell<X>;
    // compat/atomic.hpp takes only integers. Not uintptr_t: on macOS it is unsigned long,
    // and uint64_t is unsigned long long.
    using ptr_int = std::conditional_t<sizeof(void*) == sizeof(uint64_t), uint64_t, uint32_t>;

    cell<ptr_int> _ptr;
    cell<count_type> _hard_count{1};
    cell<count_type> _soft_count{1}; // weak refs, plus one held by all hard refs together

    static ptr_int to_int(bptr_base* p) noexcept { return reinterpret_cast<ptr_int>(p); }
    static bptr_base* to_ptr(ptr_int x) noexcept { return reinterpret_cast<bptr_base*>(x); }

    explicit control_block(bptr_base* ptr) noexcept: _ptr{to_int(ptr)} {}
    control_block(const control_block&) = delete;
    control_block& operator=(const control_block&) = delete;

    virtual void dispose(bptr_base* p) noexcept = 0;
    virtual void deallocate() noexcept = 0;

    bptr_base* get() const noexcept { return to_ptr(counter::load(_ptr)); }
    count_type use_count() const noexcept { return counter::load(_hard_count); }
    void add_ref() noexcept
    {
        counter::increment(_hard_count, *this);
        stats::copied();
    }
    bool add_ref_lock() noexcept;
    void weak_add_ref() noexcept { counter::increment(_soft_count, *this); }

    static void release(control_block* b) noexcept;
    static void weak_release(control_block* b) noexcept;
    static void destroy_object(control_block* b) noexcept;

protected:
    ~control_block() noexcept = default;
};

extern template struct control_block<non_atomic_refcount>;

template<typename B>
concept ControlBlock = requires(B* b, const B* cb) {
    requires std::is_same_v<decltype(cb->get()), bptr_base*>;
    cb->use_count();
    b->add_ref();
    requires std::is_same_v<decltype(b->add_ref_lock()), bool>;
    b->weak_add_ref();
    B::release(b);
    B::weak_release(b);
    B::destroy_object(b);
};

#if fm_ASAN
inline void poison(const void* p, size_t size) noexcept { __asan_poison_memory_region(p, size); }
inline void unpoison(const void* p, size_t size) noexcept { __asan_unpoison_memory_region(p, size); }
#else
inline void poison(const void*, size_t) noexcept {}
inline void unpoison(const void*, size_t) noexcept {}
#endif

template<typename Policy>
struct block_guard
{
    control_block<Policy>* b;

    ~block_guard() noexcept
    {
        if (b)
            b->deallocate();
    }
};

template<typename Policy>
struct ptr_block final : control_block<Policy>
{
    using control_block<Policy>::control_block;

    static control_block<Policy>* create(bptr_base* p) noexcept
    {
        return ::new (Policy::allocator::template allocate<ptr_block>()) ptr_block{p};
    }

    void dispose(bptr_base* p) noexcept override { delete p; }

    void deallocate() noexcept override
    {
        this->~ptr_block();
        Policy::allocator::template deallocate<ptr_block>(this);
    }
};

template<typename U, typename Policy>
struct inplace_block final : control_block<Policy>
{
    union { U _value; };

    inplace_block() noexcept: control_block<Policy>{nullptr} {}
    ~inplace_block() noexcept {}

    template<typename... Ts>
    static control_block<Policy>* create(Ts&&... args) noexcept(noexcept(U{std::declval<Ts>()...}))
    {
        auto* b = ::new (Policy::allocator::template allocate<inplace_block>()) inplace_block;
        block_guard<Policy> guard{b};
        ::new (&b->_value) U{forward<Ts>(args)...};
        guard.b = nullptr;
        Policy::counter::store(b->_ptr, b->to_int(&b->_value));
        return b;
    }

    // The storage outlives the object until the last weak_bptr drops, so ASan
    // would miss a use after destroy() without the poisoning.
    void dispose(bptr_base*) noexcept override
    {
        _value.~U();
        poison(&_value, sizeof(U));
    }

    // A pooling allocator hands the storage out again without ASan unpoisoning it.
    void deallocate() noexcept override
    {
        unpoison(&_value, sizeof(U));
        this->~inplace_block();
        Policy::allocator::template deallocate<inplace_block>(this);
    }
};

// The object starts right after the block. Padding goes before the block, so an over-aligned
// object doesn't change the distance intrusive_control_block() subtracts.
template<typename U, typename Policy>
struct intrusive_block final : control_block<Policy>
{
    using block = control_block<Policy>;
    using ptr_int = typename block::ptr_int;
    static constexpr size_t align = alignof(U) > alignof(block) ? alignof(U) : alignof(block);
    static constexpr size_t offset = (sizeof(block) + align - 1) & ~(align - 1);
    struct alignas(align) storage { unsigned char bytes[offset + sizeof(U)]; };

    intrusive_block() noexcept: block{nullptr} {}

    template<typename... Ts>
    static block* create(Ts&&... args) noexcept(noexcept(U{std::declval<Ts>()...}))
    {
        static_assert(sizeof(intrusive_block) == sizeof(block));
        auto start = reinterpret_cast<ptr_int>(Policy::allocator::template allocate<storage>());
        auto* b = ::new (reinterpret_cast<void*>(start + offset - sizeof(block))) intrusive_block;
        // Odd until the constructor returns, so bptr{this} inside it fails block_from_raw()'s check.
        Policy::counter::store(b->_ptr, (start + offset) | 1);
        block_guard<Policy> guard{b};
        auto* u = ::new (reinterpret_cast<void*>(start + offset)) U{forward<Ts>(args)...};
        fm_assert((const void*)static_cast<bptr_base*>(u) == (const void*)u);
        guard.b = nullptr;
        Policy::counter::store(b->_ptr, block::to_int(u));
        return b;
    }

    void dispose(bptr_base* p) noexcept override
    {
        auto* u = static_cast<U*>(p);
        u->~U();
        poison(u, sizeof(U));
    }

    void deallocate() noexcept override
    {
        auto start = reinterpret_cast<ptr_int>(this) + sizeof(block) - offset;
        unpoison(reinterpret_cast<void*>(start + offset), sizeof(U));
        this->~intrusive_block();
        Policy::allocator::template deallocate<storage>(reinterpret_cast<void*>(start));
    }
};

// Integer arithmetic: stepping a pointer to the object back past its start is UB.
template<typename Policy>
control_block<Policy>* intrusive_control_block(bptr_base* p) noexcept
{
    using block = control_block<Policy>;
    return std::launder(reinterpret_cast<block*>(block::to_int(p) - sizeof(block)));
}

template<typename U>
concept Intrusive = requires { typename U::intrusive_policy; } &&
                    std::is_base_of_v<intrusive_bptr_base<typename U::intrusive_policy>, U>;

} // namespace floormat::detail_bptr

namespace floormat {

template<typename T, typename Policy>
struct default_bptr_traits
{
    using ptr_block = detail_bptr::ptr_block<Policy>;
    using inplace_block = detail_bptr::inplace_block<T, Policy>;
};

template<typename T, typename Policy>
struct bptr_traits : default_bptr_traits<T, Policy> {};

} // namespace floormat

namespace floormat::detail_bptr {

template<typename U, typename Policy, typename... Ts>
CORRADE_ALWAYS_INLINE control_block<Policy>* make_block(Ts&&... args) noexcept(noexcept(U{std::declval<Ts>()...}))
{
    control_block<Policy>* b;
    if constexpr (Intrusive<U>)
    {
        static_assert(std::is_same_v<typename U::intrusive_policy, Policy>, "intrusive type used with another policy");
        b = intrusive_block<U, Policy>::create(forward<Ts>(args)...);
    }
    else
        b = bptr_traits<U, Policy>::inplace_block::create(forward<Ts>(args)...);
    Policy::stats::block_allocated();
    Policy::stats::object_constructed();
    return b;
}

template<typename U, typename Policy>
CORRADE_ALWAYS_INLINE control_block<Policy>* block_from_raw(U* ptr) noexcept
{
    if (!ptr)
        return nullptr;
    if constexpr (Intrusive<U>)
    {
        static_assert(std::is_same_v<typename U::intrusive_policy, Policy>, "intrusive type used with another policy");
        bptr_base* p = ptr;
        auto* b = intrusive_control_block<Policy>(p);
        // Fails in the object's constructor or destructor, and for an object bptr{InPlace} didn't make.
        fm_assert(Policy::counter::load(b->_ptr) == b->to_int(p));
        b->add_ref();
        return b;
    }
    else
    {
        auto* b = bptr_traits<U, Policy>::ptr_block::create(ptr);
        Policy::stats::block_allocated();
        Policy::stats::object_constructed();
        return b;
    }
}

template<typename From, typename To>
concept StaticCastable = requires(From* from, To* to) {
    static_cast<To*>(from);
    static_cast<const bptr_base*>(from);
    static_cast<const bptr_base*>(to);
};

template<typename From, typename To>
concept DerivedFrom = requires(From* from, To* to) {
    requires std::is_convertible_v<From&, To&>;
};

template<typename Y, typename T>
concept ComparableWith = requires(Y* y, T* t) { y == t; };

} // namespace floormat::detail_bptr

namespace floormat {

template<typename To, typename From, typename Policy> requires detail_bptr::StaticCastable<From, To>
basic_bptr<To, Policy> static_pointer_cast(basic_bptr<From, Policy>&& p) noexcept;

template<typename To, typename From, typename Policy> requires detail_bptr::StaticCastable<From, To>
basic_bptr<To, Policy> static_pointer_cast(const basic_bptr<From, Policy>& p) noexcept;

template<typename T, typename Policy>
class basic_bptr final // NOLINT(*-special-member-functions)
{
    using block = detail_bptr::control_block<Policy>;
    using count_type = typename Policy::counter::value_type;
    static_assert(detail_bptr::ControlBlock<block>);

    block* blk;

    template<typename Y> basic_bptr(const basic_bptr<Y, Policy>& other, std::nullptr_t) noexcept;
    template<typename Y> basic_bptr(basic_bptr<Y, Policy>&& other, std::nullptr_t) noexcept;
    template<typename Y> basic_bptr& _copy_assign(const basic_bptr<Y, Policy>& other) noexcept;
    template<typename Y> basic_bptr& _move_assign(basic_bptr<Y, Policy>&& other) noexcept;

public:
    template<typename... Ts>
    //requires std::is_constructible_v<std::remove_const_t<T>, Ts&&...>
    CORRADE_ALWAYS_INLINE explicit basic_bptr(InPlaceInitT, Ts&&... args) noexcept(noexcept(std::remove_const_t<T>{std::declval<Ts>()...}));

    CORRADE_ALWAYS_INLINE explicit basic_bptr(T* ptr) noexcept;
    CORRADE_ALWAYS_INLINE basic_bptr() noexcept;
    CORRADE_ALWAYS_INLINE basic_bptr(std::nullptr_t) noexcept; // NOLINT(*-explicit-conversions)
    ~basic_bptr() noexcept;

    basic_bptr& operator=(std::nullptr_t) noexcept;

    basic_bptr(const basic_bptr<std::remove_const_t<T>, Policy>& ptr) noexcept requires std::is_const_v<T>;
    basic_bptr(basic_bptr<std::remove_const_t<T>, Policy>&& ptr) noexcept requires std::is_const_v<T>;

    basic_bptr(const basic_bptr&) noexcept;
    basic_bptr& operator=(const basic_bptr&) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> basic_bptr(const basic_bptr<Y, Policy>&) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> basic_bptr& operator=(const basic_bptr<Y, Policy>&) noexcept;

    basic_bptr(basic_bptr&&) noexcept;
    basic_bptr& operator=(basic_bptr&&) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> basic_bptr(basic_bptr<Y, Policy>&&) noexcept;
    template<detail_bptr::DerivedFrom<T> Y> basic_bptr& operator=(basic_bptr<Y, Policy>&&) noexcept;

    void reset() noexcept;
    template<detail_bptr::DerivedFrom<T> Y> void reset(Y* ptr) noexcept;
    void destroy() noexcept;
    void swap(basic_bptr& other) noexcept;
    count_type use_count() const noexcept;
    // true after destroy() through another copy, unlike operator bool
    bool has_block() const noexcept;

    T* get() const noexcept;
    T* operator->() const noexcept;
    T& operator*() const noexcept;

    explicit operator bool() const noexcept;

    bool operator==(const basic_bptr<const T, Policy>& other) const noexcept;
    bool operator==(const basic_bptr<T, Policy>& other) const noexcept requires (!std::is_const_v<T>);
    template<detail_bptr::ComparableWith<T> Y> bool operator==(const basic_bptr<Y, Policy>& other) const noexcept;
    bool operator==(const std::nullptr_t& other) const noexcept;

    template<typename U, typename P> friend class basic_bptr;
    template<typename U, typename P> friend class basic_weak_bptr;

    template<typename To, typename From, typename P>
    requires detail_bptr::StaticCastable<From, To>
    friend basic_bptr<To, P> static_pointer_cast(basic_bptr<From, P>&& p) noexcept;

    template<typename To, typename From, typename P>
    requires detail_bptr::StaticCastable<From, To>
    friend basic_bptr<To, P> static_pointer_cast(const basic_bptr<From, P>& p) noexcept;
};

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

template<typename T, typename Policy>
template<typename... Ts>
//requires std::is_constructible_v<std::remove_const_t<T>, Ts&&...>
basic_bptr<T, Policy>::basic_bptr(InPlaceInitT, Ts&&... args) noexcept(noexcept(std::remove_const_t<T>{std::declval<Ts>()...})):
    blk{detail_bptr::make_block<std::remove_const_t<T>, Policy>(forward<Ts>(args)...)}
{}

template<typename T, typename Policy> basic_bptr<T, Policy>::basic_bptr(std::nullptr_t) noexcept: blk{nullptr} {}
template<typename T, typename Policy> basic_bptr<T, Policy>::basic_bptr() noexcept: basic_bptr{nullptr} {}

template<typename T, typename Policy>
basic_bptr<T, Policy>::basic_bptr(T* ptr) noexcept:
    blk{detail_bptr::block_from_raw<std::remove_const_t<T>, Policy>(const_cast<std::remove_const_t<T>*>(ptr))}
{}

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

// Defined in the header: the explicit instantiations in each class's .cpp skip member templates.
template<typename T, typename P>
template<detail_bptr::ComparableWith<T> Y>
bool basic_bptr<T, P>::operator==(const basic_bptr<Y, P>& other) const noexcept
{
    return (blk ? blk->get() : nullptr) == (other.blk ? other.blk->get() : nullptr);
}

} // namespace floormat
