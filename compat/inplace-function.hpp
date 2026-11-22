#pragma once

#include "fwd/construct_at.hpp"
#include "inplace-function-fwd.hpp"
#include "assert.hpp"
#include "common-function.hpp"
#include "invoke.hpp"
#include "function-view-fwd.hpp"
#include <new>
#include <type_traits>
#include <utility>

#define FM_INPLACE_FUNCTION_QUALIFIERS(X)              \
    X(, , false, false, none)                          \
    X(const, , true, false, none)                      \
    X(volatile, , false, true, none)                   \
    X(const volatile, , true, true, none)              \
    X(, &, false, false, lvalue)                       \
    X(const, &, true, false, lvalue)                   \
    X(volatile, &, false, true, lvalue)                \
    X(const volatile, &, true, true, lvalue)           \
    X(, &&, false, false, rvalue)                      \
    X(const, &&, true, false, rvalue)                  \
    X(volatile, &&, false, true, rvalue)               \
    X(const volatile, &&, true, true, rvalue)

namespace floormat::detail::inplace_function_impl {

template<bool Const, bool Volatile, typename T>
using add_cv = std::conditional_t<Const, std::conditional_t<Volatile, const volatile T, const T>,
                                         std::conditional_t<Volatile, volatile T, T>>;

using common_function_impl::reference_converts_from_temporary;
using common_function_impl::pointers_equal;
using common_function_impl::is_empty_callable;

template<typename T, typename U> struct same_signature
{
    static constexpr bool value = false;
};
template<typename T, bool C1, bool Const, bool Volatile, ref_qualifier Ref, bool Noexcept,
         usize Capacity1, usize Alignment1, typename R, typename... Args>
struct same_signature<T, function_base<C1, Const, Volatile, Ref, Noexcept, Capacity1, Alignment1, R, Args...>>
{
    template<bool C, usize Capacity, usize Alignment>
    static void test(const function_base<C, Const, Volatile, Ref, Noexcept, Capacity, Alignment, R, Args...>*);
    static constexpr bool value = requires(const T* p) { test(p); };
};

// Class types go by reference, so that a signature can name an incomplete one.
template<typename T> using param = std::conditional_t<std::is_reference_v<T> || std::is_scalar_v<T>, T, T&&>;

template<typename F, typename Self, typename Call, bool Copyable, bool Noexcept, typename R, usize Capacity, usize Alignment, typename... Args>
concept fits = !same_signature<std::remove_cvref_t<F>, Self>::value &&
               !function_view_impl::is_function_view<std::remove_cvref_t<F>> &&
               sizeof(std::decay_t<F>) <= Capacity &&
               alignof(std::decay_t<F>) <= Alignment &&
               std::is_nothrow_move_constructible_v<std::decay_t<F>> &&
               std::is_nothrow_destructible_v<std::decay_t<F>> &&
               std::is_constructible_v<std::decay_t<F>, F> &&
               (!Copyable || std::is_copy_constructible_v<std::decay_t<F>>) &&
               (Noexcept ? std::is_nothrow_invocable_r_v<R, Call, Args...> : std::is_invocable_r_v<R, Call, Args...>) &&
               !reference_converts_from_temporary<R, std::invoke_result_t<Call, Args...>>;

// Constant evaluation can't create an object inside a byte array, so there the callable lives
// on the heap, freed before the evaluation ends.
struct holder_base {};
template<typename F>
struct holder final : holder_base
{
    F value;
    template<typename G> constexpr explicit holder(G&& g): value(std::forward<G>(g)) {}
    // MSVC 19.52 can't constant-evaluate a defaulted one.
    constexpr ~holder() noexcept {}
};

// Independent of the capacity, so that a target moves between capacities.
union storage_ptr
{
    holder_base* heap;
    void* buf;
};

template<typename F> constexpr F& target(storage_ptr s) noexcept
{
    if consteval
    {
        return static_cast<holder<F>*>(s.heap)->value;
    }
    else
    {
        return *std::launder(static_cast<F*>(s.buf));
    }
}

struct vtable
{
    // Runtime only. Constant evaluation moves the `_heap` pointer instead.
    using move_fn = void(storage_ptr to, storage_ptr from) noexcept;
    using destroy_fn = void(storage_ptr) noexcept;
    // Returns the new holder during constant evaluation.
    using copy_fn = holder_base*(storage_ptr to, storage_ptr from);

    move_fn* move;
    destroy_fn* destroy;
    copy_fn* copy;

    constexpr vtable(move_fn* move, destroy_fn* destroy, copy_fn* copy) noexcept: move{move}, destroy{destroy}, copy{copy} {}
};

template<typename F> void move_target(storage_ptr to, storage_ptr from) noexcept
{
    F& source = target<F>(from);
    std::construct_at(static_cast<F*>(to.buf), std::move(source));
    std::destroy_at(std::addressof(source));
}

template<typename F> constexpr void destroy_target(storage_ptr s) noexcept
{
    if consteval
    {
        delete static_cast<holder<F>*>(s.heap);
    }
    else
    {
        std::destroy_at(std::addressof(target<F>(s)));
    }
}

template<typename F> constexpr holder_base* copy_target(storage_ptr to, storage_ptr from)
{
    if consteval
    {
        return new holder<F>{std::as_const(target<F>(from))};
    }
    else
    {
        std::construct_at(static_cast<F*>(to.buf), std::as_const(target<F>(from)));
        return nullptr;
    }
}

constexpr void move_empty(storage_ptr, storage_ptr) noexcept {}
constexpr void destroy_empty(storage_ptr) noexcept {}
constexpr holder_base* copy_empty(storage_ptr, storage_ptr) noexcept { return nullptr; }

template<typename F> consteval vtable::copy_fn* copy_of()
{
    if constexpr (std::is_copy_constructible_v<F>)
        return &copy_target<F>;
    else
        return nullptr;
}

constexpr inline vtable empty_table{ &move_empty, &destroy_empty, &copy_empty };
template<typename F> constexpr inline vtable table{ &move_target<F>, &destroy_target<F>, copy_of<F>() };

template<bool Const, bool Volatile, ref_qualifier Ref, bool Noexcept, typename R, typename... Args>
struct invoker
{
    template<typename F>
    using callable = std::conditional_t<Ref == ref_qualifier::rvalue, add_cv<Const, Volatile, F>&&, add_cv<Const, Volatile, F>&>;

    using fn = R(storage_ptr, param<Args>...) noexcept(Noexcept);

    template<typename F> static constexpr R invoke(storage_ptr s, param<Args>... args) noexcept(Noexcept)
    {
        if constexpr (std::is_void_v<R>)
            return static_cast<void>(floormat::invoke(static_cast<callable<F>>(target<F>(s)), std::forward<Args>(args)...));
        else
            return floormat::invoke(static_cast<callable<F>>(target<F>(s)), std::forward<Args>(args)...);
    }

    [[noreturn]] static R invoke_empty(storage_ptr, param<Args>...) noexcept { fm_abort("called an empty inplace_function"); }
};

template<bool Copyable, bool Const, bool Volatile, ref_qualifier Ref, bool Noexcept, usize Capacity, usize Alignment, typename R, typename... Args>
class function_base
{
    static_assert(Capacity > 0);

    template<bool, bool, bool, ref_qualifier, bool, usize, usize, typename, typename...> friend class function_base;

    using invoker_type = invoker<Const, Volatile, Ref, Noexcept, R, Args...>;
    template<typename F> using callable = invoker_type::template callable<F>;
    template<bool C, usize Cap, usize Al> using similar = function_base<C, Const, Volatile, Ref, Noexcept, Cap, Al, R, Args...>;

    // Outside the vtable, so that a call loads one pointer.
    invoker_type::fn* _invoke = &invoker_type::invoke_empty;
    union
    {
        alignas(Alignment) unsigned char _buf[Capacity];
        holder_base* _heap;
    };
    // Apart from `_invoke`, because compilers copy adjacent pointers with one 16-byte move, which
    // costs about 30 extra cycles when it crosses a page.
    const vtable* _vt = &empty_table;

    // Constant evaluation can't read through a volatile glvalue, so there the members are read
    // through a const_cast.
    template<typename S> static constexpr invoker_type::fn* invoke_of(S& self) noexcept
    {
        if consteval
        {
            return const_cast<const std::remove_cv_t<S>&>(self)._invoke;
        }
        else
        {
            return self._invoke;
        }
    }

    template<typename S> static constexpr storage_ptr storage(S& self) noexcept
    {
        auto& s = const_cast<std::remove_cv_t<S>&>(self);
        if consteval
        {
            return {.heap = s._heap};
        }
        else
        {
            return {.buf = s._buf};
        }
    }

    template<typename F> constexpr void construct(F&& f) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F>)
    {
        using D = std::decay_t<F>;
        if consteval
        {
            _heap = new holder<D>{std::forward<F>(f)};
        }
        else
        {
            std::construct_at(static_cast<D*>(static_cast<void*>(_buf)), std::forward<F>(f));
        }
    }

    constexpr void destroy() noexcept
    {
        _vt->destroy(storage(*this));
        if consteval
        {
            _heap = nullptr;
        }
    }

    constexpr void clear() noexcept
    {
        _invoke = &invoker_type::invoke_empty;
        _vt = &empty_table;
    }

    // Expects `_invoke` and `_vt` already copied from `other`.
    template<typename Other> constexpr void relocate(Other& other) noexcept
    {
        if consteval
        {
            _heap = other._heap;
            other._heap = nullptr;
        }
        else
        {
            _vt->move(storage(*this), storage(other));
        }
        other.clear();
    }

    template<typename Other> constexpr void take(Other& other) noexcept
    {
        _invoke = other._invoke;
        _vt = other._vt;
        relocate(other);
    }

    // A temporary aligned to its size rounded up to a power of two, up to 64, sits in one cache line,
    // so moving a target through it never splits a line or a page.
    static consteval usize local_alignment() noexcept
    {
        usize a = alignof(function_base);
        while (a < sizeof(function_base) && a < 64)
            a *= 2;
        return a;
    }

    // The old target may own the source, as when a running continuation assigns its successor,
    // so the source moves out first.
    template<typename Source> constexpr void replace(Source&& source) noexcept(std::is_nothrow_constructible_v<function_base, Source>)
    {
        alignas(local_alignment()) function_base next{std::forward<Source>(source)};
        destroy();
        take(next);
    }

    template<typename Other> constexpr void copy_from(const Other& other)
    {
        if consteval
        {
            _heap = other._vt->copy({}, storage(other));
        }
        else
        {
            other._vt->copy(storage(*this), storage(other));
        }
        _invoke = other._invoke;
        _vt = other._vt;
    }

public:
    // gcc rejects a constexpr variable whose union has no active member.
    constexpr function_base() noexcept: _heap{nullptr} {}
    constexpr function_base(decltype(nullptr)) noexcept: _heap{nullptr} {}

    template<typename F>
    requires fits<F, function_base, callable<std::decay_t<F>>, Copyable, Noexcept, R, Capacity, Alignment, Args...>
    constexpr function_base(F&& f) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F>):
        _invoke{&invoker_type::template invoke<std::decay_t<F>>}, _vt{&table<std::decay_t<F>>}
    {
        if (is_empty_callable(f))
        {
            _heap = nullptr;
            clear();
        }
        else
        {
            construct(std::forward<F>(f));
        }
    }

    constexpr function_base(function_base&& other) noexcept: _invoke{other._invoke}, _vt{other._vt} { relocate(other); }

    template<bool C, usize Cap, usize Al>
    requires (Cap <= Capacity && Al <= Alignment && (C || !Copyable))
    constexpr function_base(similar<C, Cap, Al>&& other) noexcept: _invoke{other._invoke}, _vt{other._vt} { relocate(other); }

    constexpr function_base(const function_base& other) requires Copyable { copy_from(other); }
    function_base(const function_base&) requires (!Copyable) = delete;

    template<usize Cap, usize Al>
    requires (Cap <= Capacity && Al <= Alignment)
    constexpr function_base(const similar<true, Cap, Al>& other) { copy_from(other); }

    constexpr function_base& operator=(function_base&& other) noexcept
    {
        replace(std::move(other));
        return *this;
    }

    template<bool C, usize Cap, usize Al>
    requires (Cap <= Capacity && Al <= Alignment && (C || !Copyable))
    constexpr function_base& operator=(similar<C, Cap, Al>&& other) noexcept
    {
        replace(std::move(other));
        return *this;
    }

    constexpr function_base& operator=(const function_base& other) requires Copyable
    {
        replace(other);
        return *this;
    }
    function_base& operator=(const function_base&) requires (!Copyable) = delete;

    template<usize Cap, usize Al>
    requires (Cap <= Capacity && Al <= Alignment)
    constexpr function_base& operator=(const similar<true, Cap, Al>& other)
    {
        replace(other);
        return *this;
    }

    constexpr function_base& operator=(std::nullptr_t) noexcept
    {
        destroy();
        clear();
        return *this;
    }

    template<typename F>
    requires fits<F, function_base, callable<std::decay_t<F>>, Copyable, Noexcept, R, Capacity, Alignment, Args...>
    constexpr function_base& operator=(F&& f) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F>)
    {
        using D = std::decay_t<F>;
        if (is_empty_callable(f))
        {
            destroy();
            clear();
        }
        else
        {
            // Same order as replace(), with the source's type known.
            D next(std::forward<F>(f));
            destroy();
            construct(std::move(next));
            _invoke = &invoker_type::template invoke<D>;
            _vt = &table<D>;
        }
        return *this;
    }

    constexpr ~function_base() noexcept { _vt->destroy(storage(*this)); }

    constexpr void swap(function_base& other) noexcept
    {
        if (this != &other)
        {
            alignas(local_alignment()) function_base tmp{std::move(other)};
            other.take(*this);
            take(tmp);
        }
    }

    friend constexpr void swap(function_base& a, function_base& b) noexcept { a.swap(b); }

#define FM_INPLACE_FUNCTION_CALL(CV, REF, IS_CONST, IS_VOLATILE, REF_KIND)                         \
    constexpr R operator()(Args... args) CV REF noexcept(Noexcept)                                  \
        requires (Const == IS_CONST && Volatile == IS_VOLATILE && Ref == ref_qualifier::REF_KIND)   \
    {                                                                                               \
        return invoke_of(*this)(storage(*this), std::forward<Args>(args)...);                       \
    }
    FM_INPLACE_FUNCTION_QUALIFIERS(FM_INPLACE_FUNCTION_CALL)
#undef FM_INPLACE_FUNCTION_CALL

    constexpr bool empty() const noexcept { return pointers_equal(invoke_of(*this), &invoker_type::invoke_empty); }
    constexpr bool empty() const volatile noexcept { return pointers_equal(invoke_of(*this), &invoker_type::invoke_empty); }
    constexpr explicit operator bool() const noexcept { return !empty(); }
    constexpr explicit operator bool() const volatile noexcept { return !empty(); }

    friend constexpr bool operator==(const function_base& f, std::nullptr_t) noexcept { return f.empty(); }
};

} // namespace floormat::detail::inplace_function_impl

namespace floormat {

#define FM_INPLACE_FUNCTION_CLASS(NAME, COPYABLE, CV, REF, IS_CONST, IS_VOLATILE, REF_KIND)                     \
    template<typename R, typename... Args, bool Noexcept, usize Capacity, usize Alignment>                      \
    class NAME<R(Args...) CV REF noexcept(Noexcept), Capacity, Alignment> final:                                \
        public detail::inplace_function_impl::function_base<COPYABLE, IS_CONST, IS_VOLATILE,                    \
            detail::inplace_function_impl::ref_qualifier::REF_KIND, Noexcept, Capacity, Alignment, R, Args...>  \
    {                                                                                                           \
        using base_type = detail::inplace_function_impl::function_base<COPYABLE, IS_CONST, IS_VOLATILE,         \
            detail::inplace_function_impl::ref_qualifier::REF_KIND, Noexcept, Capacity, Alignment, R, Args...>; \
                                                                                                                \
    public:                                                                                                     \
        using base_type::base_type;                                                                             \
        using base_type::operator=;                                                                             \
    };
#define FM_INPLACE_FUNCTION_SIGNATURE(CV, REF, IS_CONST, IS_VOLATILE, REF_KIND)                     \
    FM_INPLACE_FUNCTION_CLASS(inplace_function, false, CV, REF, IS_CONST, IS_VOLATILE, REF_KIND)    \
    FM_INPLACE_FUNCTION_CLASS(inplace_copyable_function, true, CV, REF, IS_CONST, IS_VOLATILE, REF_KIND)
FM_INPLACE_FUNCTION_QUALIFIERS(FM_INPLACE_FUNCTION_SIGNATURE)
#undef FM_INPLACE_FUNCTION_SIGNATURE
#undef FM_INPLACE_FUNCTION_CLASS

} // namespace floormat

#undef FM_INPLACE_FUNCTION_QUALIFIERS
