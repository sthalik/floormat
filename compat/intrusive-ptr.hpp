#pragma once
#include "compat/assert.hpp"
#include <compare>

namespace floormat::iptr { struct non_atomic_u32_tag{}; }

namespace floormat {

template<typename Tag, typename T> class basic_iptr;
template<typename T> using local_iptr = basic_iptr<iptr::non_atomic_u32_tag, T>;

} // namespace floormat

namespace floormat::iptr  {

template<typename Tag, typename T> struct refcount_traits;
template<typename Tag, typename T> struct refcount_access;
template<typename Tag, typename T> struct refcount_ops;

template<typename T>
struct refcount_traits<non_atomic_u32_tag, T>
{
    refcount_traits() = delete;
    using counter_type = uint32_t;
    using size_type = size_t;
    using access_t = refcount_access<non_atomic_u32_tag, T>;
};

template<typename Tag, typename T>
struct refcount_ops
{
    refcount_ops() = delete;

    using traits = refcount_traits<Tag, T>;
    using counter_type = typename traits::counter_type;
    using size_type = typename traits::size_type;
    using access_t = typename traits::access_t;

#ifndef FM_NO_DEBUG
    using Tref = T*&;
#else
    using Tref = T*;
#endif

    static constexpr inline auto incr(T* ptr) noexcept -> size_type;
    static constexpr inline auto decr(Tref ptr) noexcept -> size_type;
    static constexpr inline auto count(T* ptr) noexcept -> size_type;
    static constexpr inline void init_to_1(T* ptr) noexcept;
};

template<typename Tag, typename T>
struct refcount_access
{
    using counter_type = refcount_traits<Tag, T>::counter_type;
    refcount_access() = delete;

    static constexpr auto access(T* ptr) noexcept -> counter_type&; // todo this should return the iptr's control block. it has to contain the original pointer (because the casted ptr (or its base class, when casted to iptr<base>) might not have a virtual dtor), shared count, and (shared+weak) count.
    template<typename Y> static constexpr Y* checked_cast(const T* ptr) noexcept; // todo
};

template<typename Tag, typename T>
constexpr auto refcount_ops<Tag, T>::incr(T* ptr) noexcept -> size_type
{
    fm_debug_assert(ptr != nullptr);
    counter_type& ctr{access_t::access(ptr)};
    fm_debug_assert(size_type{ctr} > size_type{0});
    size_type ret{++ctr};
    return ret;
}

template<typename Tag, typename T>
constexpr auto refcount_ops<Tag, T>::decr(Tref ptr) noexcept -> size_type
{
    static_assert(std::is_nothrow_destructible_v<T>);

    fm_debug_assert(ptr != nullptr);
    counter_type& ctr{access_t::access(ptr)};
    fm_debug_assert(size_type{ctr} > size_type{0});
    size_type ret{--ctr};
    if (ret == size_type{0})
    {
        delete ptr;
        if constexpr(std::is_reference_v<Tref>)
            ptr = reinterpret_cast<T*>((uintptr_t)-1);
    }
    return ret;
}

template<typename Tag, typename T>
constexpr auto refcount_ops<Tag, T>::count(T* ptr) noexcept -> size_type
{
    if (!CORRADE_CONSTEVAL)
        fm_debug_assert((void*)ptr != (void*)-1);
    if (!ptr) [[unlikely]]
        return 0;
    counter_type& ctr{access_t::access(ptr)};
    fm_debug_assert(size_type{ctr} > size_type{0});
    size_type ret{ctr};
    return ret;
}

template<typename Tag, typename T>
constexpr void refcount_ops<Tag, T>::init_to_1(T* ptr) noexcept
{
    fm_debug_assert(ptr != nullptr);
    counter_type& ctr{access_t::access(ptr)};
    fm_debug_assert(size_type{ctr} == size_type{0});
    ctr = size_type{1};
}

template<typename Tag, typename T>
consteval bool check_traits()
{
    using traits = ::floormat::iptr::refcount_traits<Tag, T>;
    using counter_type = typename traits::counter_type;
    using size_type = typename traits::size_type;
    using access_t = ::floormat::iptr::refcount_access<Tag, T>;
    using ops_t = ::floormat::iptr::refcount_ops<Tag, T>;

    static_assert(requires (counter_type& ctr) {
        requires std::is_arithmetic_v<size_type>;
        requires std::is_unsigned_v<size_type>;
        requires std::is_fundamental_v<size_type>;
        requires noexcept(size_type{ctr});
        { size_t{size_type{ctr}} };
        { ctr = size_type{1} };
        { size_type{++ctr} };
        { size_type{--ctr} };
        requires noexcept(size_t{size_type{ctr}});
        requires noexcept(ctr = size_type{1});
        requires noexcept(size_type{++ctr});
        requires noexcept(size_type{--ctr});
        requires std::is_nothrow_destructible_v<T>;
        requires sizeof(access_t) != 0;
        requires sizeof(ops_t) != 0;
    });

    return true;
}

} // namespace floormat::iptr



// ----- macros -----

#define fm_template template<typename Tag, typename T>
#define fm_basic_iptr basic_iptr<Tag, T>

#ifndef FM_IPTR_USE_CONSTRUCT_AT // todo finish it
#define FM_IPTR_USE_CONSTRUCT_AT 0
#endif

// ----- basic_iptr -----

namespace floormat {

template<typename Tag, typename T>
class basic_iptr final
{
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_const_v<T>); // todo, modify only refcount

    using traits = ::floormat::iptr::refcount_traits<Tag, T>;
    using counter_type = typename traits::counter_type;
    using size_type = typename traits::size_type;
    using access_t = ::floormat::iptr::refcount_access<Tag, T>;
    using ops_t = ::floormat::iptr::refcount_ops<Tag, T>;
    static_assert(::floormat::iptr::check_traits<Tag, T>());

    T* _ptr{nullptr}; // todo control block

    // todo use std::construct_at as it has a constexpr exception.
    // ...but it requires <memory> :( — maybe use an ifdef?

public:
    constexpr basic_iptr() noexcept;
    constexpr basic_iptr(std::nullptr_t) noexcept;
    constexpr basic_iptr& operator=(std::nullptr_t) noexcept;
    explicit constexpr basic_iptr(T* ptr) noexcept;
    constexpr basic_iptr(basic_iptr&& other) noexcept;
    constexpr basic_iptr(const basic_iptr& other) noexcept;
    constexpr basic_iptr& operator=(basic_iptr&& other) noexcept;
    constexpr basic_iptr& operator=(const basic_iptr& other) noexcept;
    constexpr ~basic_iptr() noexcept;

    template<typename... Ts> requires std::is_constructible_v<T, Ts&&...>
    constexpr basic_iptr(InPlaceInitT, Ts&&... args) // NOLINT(*-missing-std-forward)
    noexcept(std::is_nothrow_constructible_v<T, Ts&&...>);

    constexpr inline void reset() noexcept;
    constexpr void reset(T* ptr) noexcept; // todo casts
    constexpr void swap(basic_iptr& other) noexcept;
    constexpr T* get() const noexcept;
    constexpr inline T& operator*() const noexcept;
    constexpr inline T* operator->() const noexcept;

    constexpr size_type use_count() const noexcept;
    explicit constexpr operator bool() const noexcept;

    template<typename TAG, typename TYPE>
    friend constexpr bool operator==(const basic_iptr<TAG, TYPE>& a, const basic_iptr<TAG, TYPE>& b) noexcept;

    template<typename TAG, typename TYPE>
    friend constexpr std::strong_ordering operator<=>(const basic_iptr<TAG, TYPE>& a, const basic_iptr<TAG, TYPE>& b) noexcept;
};

// ----- constructors -----

template<typename Tag, typename T> constexpr fm_basic_iptr::basic_iptr() noexcept = default;
template<typename Tag, typename T> constexpr fm_basic_iptr::basic_iptr(std::nullptr_t) noexcept {}

fm_template constexpr fm_basic_iptr& fm_basic_iptr::operator=(std::nullptr_t) noexcept
{
    if (_ptr)
    {
        ops_t::decr(_ptr);
        _ptr = nullptr;
    }
    return *this;
}

fm_template constexpr fm_basic_iptr::basic_iptr(fm_basic_iptr&& other) noexcept: _ptr{other._ptr} { other._ptr = nullptr; }

fm_template constexpr fm_basic_iptr::basic_iptr(const fm_basic_iptr& other) noexcept:
    _ptr{other._ptr}
{
    if (_ptr)
        ops_t::incr(_ptr);
}

fm_template constexpr fm_basic_iptr::basic_iptr(T* ptr) noexcept
{
    ops_t::init_to_1(ptr);
}

// ----- destructor -----

fm_template constexpr fm_basic_iptr::~basic_iptr() noexcept { reset(); }

// ----- assignment operators -----

fm_template constexpr fm_basic_iptr& fm_basic_iptr::operator=(fm_basic_iptr&& other) noexcept
{
    if (_ptr != other._ptr)
    {
        reset();
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    return *this;
}

fm_template constexpr fm_basic_iptr& fm_basic_iptr::operator=(const fm_basic_iptr& other) noexcept
{
#ifdef __CLION_IDE__
    if (&other == this)
        return *this;
#endif
    if (other._ptr != _ptr) [[likely]]
    {
        if (_ptr)
            ops_t::decr(_ptr);
        _ptr = other._ptr;
        if (_ptr)
            ops_t::incr(_ptr);
    }
    return *this;
}

fm_template template<typename... Ts>
requires std::is_constructible_v<T, Ts&&...>
constexpr fm_basic_iptr::basic_iptr(InPlaceInitT, Ts&&... args) // NOLINT(*-missing-std-forward)
noexcept(std::is_nothrow_constructible_v<T, Ts&&...>):
    _ptr{new T{forward<Ts...>(args...)}}
{
    ops_t::init_to_1(_ptr);
}

fm_template constexpr void fm_basic_iptr::reset() noexcept { if (_ptr) ops_t::decr(_ptr); }
fm_template constexpr void fm_basic_iptr::reset(T* ptr) noexcept { reset(); _ptr = ptr; }
fm_template constexpr void fm_basic_iptr::swap(basic_iptr& other) noexcept { auto p = _ptr; _ptr = other._ptr; other._ptr = p; }

fm_template constexpr T* fm_basic_iptr::get() const noexcept
{
    fm_debug_assert((void*)_ptr != (void*)-1); // NOLINT(*-no-int-to-ptr)
    return _ptr;
}

fm_template constexpr T& fm_basic_iptr::operator*() const noexcept { return *get(); }
fm_template constexpr T* fm_basic_iptr::operator->() const noexcept { return get(); }

fm_template constexpr auto fm_basic_iptr::use_count() const noexcept -> size_type
{
    if (!CORRADE_CONSTEVAL)
        fm_debug_assert((void*)_ptr != (void*)-1);
    return ops_t::count(_ptr);
}

fm_template constexpr fm_basic_iptr::operator bool() const noexcept
{
    if (!CORRADE_CONSTEVAL)
        fm_debug_assert((void*)_ptr != (void*)-1);
    return _ptr != nullptr;
}

fm_template constexpr bool operator==(const fm_basic_iptr& a, const fm_basic_iptr& b) noexcept
{
    if (!CORRADE_CONSTEVAL)
    {
        fm_debug_assert((void*)a._ptr != (void*)-1);
        fm_debug_assert((void*)b._ptr != (void*)-1);
    }
    return a._ptr == b._ptr;
}

fm_template constexpr std::strong_ordering operator<=>(const fm_basic_iptr& a, const fm_basic_iptr& b) noexcept
{
    return a._ptr <=> b._ptr;
}

#undef fm_template
#undef fm_basic_iptr

} // namespace floormat
