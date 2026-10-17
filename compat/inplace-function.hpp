#pragma once
#include "compat/inplace-function-fwd.hpp"
#include "compat/assert.hpp"
#include "compat/function2.fwd.hpp"
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace floormat::detail::inplace_function_impl {

template<bool Const, bool Volatile, typename T>
using add_cv = std::conditional_t<Const, std::conditional_t<Volatile, const volatile T, const T>,
                                         std::conditional_t<Volatile, volatile T, T>>;

template<typename T> constexpr inline bool is_fu2_function = false;
template<typename Config, typename Property>
constexpr inline bool is_fu2_function<fu2::detail::function<Config, Property>> = true;

template<typename T, typename U> constexpr inline bool same_signature = false;
template<bool C1, bool C2, bool Const, bool Volatile, ref_qualifier Ref, bool Noexcept,
         size_t Capacity1, size_t Alignment1, size_t Capacity2, size_t Alignment2, typename R, typename... Args>
constexpr inline bool same_signature<function<C1, Const, Volatile, Ref, Noexcept, Capacity1, Alignment1, R, Args...>,
                                     function<C2, Const, Volatile, Ref, Noexcept, Capacity2, Alignment2, R, Args...>> = true;

// Class types go by reference, so that a signature can name an incomplete one.
template<typename T> using param = std::conditional_t<std::is_reference_v<T> || std::is_scalar_v<T>, T, T&&>;

template<typename T> constexpr inline bool is_reference_wrapper = !std::is_same_v<std::unwrap_reference_t<T>, T>;

// MSVC 19.52 can't constant-evaluate a write through `call().*f`, so the object comes in as a
// parameter.
template<typename M, typename C, typename O, typename... Args>
constexpr decltype(auto) invoke_member_of(M C::* f, O&& object, Args&&... args)
{
    if constexpr (std::is_function_v<M>)
        return (static_cast<O&&>(object).*f)(std::forward<Args>(args)...);
    else
        return static_cast<O&&>(object).*f;
}

// INVOKE ([func.require]) for a member pointer, without <functional>.
template<typename M, typename C, typename T, typename... Args>
constexpr decltype(auto) invoke_member(M C::* f, T&& t, Args&&... args)
{
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_base_of_v<C, U>)
        return invoke_member_of(f, static_cast<T&&>(t), std::forward<Args>(args)...);
    else if constexpr (is_reference_wrapper<U>)
        return invoke_member_of(f, t.get(), std::forward<Args>(args)...);
    else
        return invoke_member_of(f, *static_cast<T&&>(t), std::forward<Args>(args)...);
}

template<typename F, typename... Args>
constexpr decltype(auto) invoke_target(F&& f, Args&&... args)
{
    if constexpr (std::is_member_pointer_v<std::remove_cvref_t<F>>)
        return invoke_member(f, std::forward<Args>(args)...);
    else
        return std::forward<F>(f)(std::forward<Args>(args)...);
}

#ifdef __cpp_lib_reference_from_temporary
template<typename R, typename T>
constexpr inline bool reference_converts_from_temporary = std::reference_converts_from_temporary_v<R, T>;
#else
// libc++ 19 lacks the trait. Later libc++ has it but doesn't define the feature macro.
template<typename R, typename T>
constexpr inline bool reference_converts_from_temporary = __reference_converts_from_temporary(R, T);
#endif

template<typename F, typename Self, typename Call, bool Copyable, bool Noexcept, typename R, size_t Capacity, size_t Alignment, typename... Args>
concept fits = !same_signature<std::remove_cvref_t<F>, Self> &&
               !is_fu2_function<std::remove_cvref_t<F>> &&
               sizeof(std::decay_t<F>) <= Capacity &&
               alignof(std::decay_t<F>) <= Alignment &&
               std::is_nothrow_move_constructible_v<std::decay_t<F>> &&
               std::is_nothrow_destructible_v<std::decay_t<F>> &&
               std::is_constructible_v<std::decay_t<F>, F> &&
               (!Copyable || std::is_copy_constructible_v<std::decay_t<F>>) &&
               (Noexcept ? std::is_nothrow_invocable_r_v<R, Call, Args...> : std::is_invocable_r_v<R, Call, Args...>) &&
               !reference_converts_from_temporary<R, std::invoke_result_t<Call, Args...>>;

// Under UBSan, gcc can't fold a comparison of a function's address to another function's or to
// null in constant evaluation (PR 71962). The ones it can't fold are false.
template<typename P, typename Q> constexpr bool pointers_equal(P a, Q b) noexcept
{
#if defined __GNUC__ && !defined __clang__
    if consteval
    {
        if (!__builtin_constant_p(a == b))
            return false;
    }
#endif
    return a == b;
}

// As in fu2, a callable that converts to false, such as a null pointer or an empty function,
// leaves the function empty.
template<typename F> constexpr bool is_empty_callable(F& f)
{
    if constexpr (std::is_pointer_v<std::remove_cv_t<F>> || std::is_member_pointer_v<std::remove_cv_t<F>>)
        return pointers_equal(f, nullptr);
    // A captureless lambda, through its function pointer.
    else if constexpr (std::is_class_v<F> && requires (F& g) { requires std::is_pointer_v<decltype(+g)>; })
        return pointers_equal(+f, nullptr);
    else if constexpr (std::is_class_v<F> && requires (F& g) { static_cast<bool>(g); })
        return !static_cast<bool>(f);
    else
        return false;
}

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
            return static_cast<void>(invoke_target(static_cast<callable<F>>(target<F>(s)), std::forward<Args>(args)...));
        else
            return invoke_target(static_cast<callable<F>>(target<F>(s)), std::forward<Args>(args)...);
    }

    [[noreturn]] static R invoke_empty(storage_ptr, param<Args>...) noexcept { fm_abort("called an empty inplace_function"); }
};

template<bool Copyable, bool Const, bool Volatile, ref_qualifier Ref, bool Noexcept, size_t Capacity, size_t Alignment, typename R, typename... Args>
class function final
{
    static_assert(Capacity > 0);

    template<bool, bool, bool, ref_qualifier, bool, size_t, size_t, typename, typename...> friend class function;

    using invoker_type = invoker<Const, Volatile, Ref, Noexcept, R, Args...>;
    template<typename F> using callable = typename invoker_type::template callable<F>;
    template<bool C, size_t Cap, size_t Al> using similar = function<C, Const, Volatile, Ref, Noexcept, Cap, Al, R, Args...>;

    // Outside the vtable, so that a call loads one pointer.
    typename invoker_type::fn* _invoke = &invoker_type::invoke_empty;
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
    template<typename S> static constexpr typename invoker_type::fn* invoke_of(S& self) noexcept
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
    static consteval size_t local_alignment() noexcept
    {
        size_t a = alignof(function);
        while (a < sizeof(function) && a < 64)
            a *= 2;
        return a;
    }

    // The old target may own the source, as when a running continuation assigns its successor,
    // so the source moves out first.
    template<typename Source> constexpr void replace(Source&& source) noexcept(std::is_nothrow_constructible_v<function, Source>)
    {
        alignas(local_alignment()) function next{std::forward<Source>(source)};
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
    constexpr function() noexcept: _heap{nullptr} {}
    constexpr function(std::nullptr_t) noexcept: _heap{nullptr} {}

    template<typename F>
    requires fits<F, function, callable<std::decay_t<F>>, Copyable, Noexcept, R, Capacity, Alignment, Args...>
    constexpr function(F&& f) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F>):
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

    constexpr function(function&& other) noexcept: _invoke{other._invoke}, _vt{other._vt} { relocate(other); }

    template<bool C, size_t Cap, size_t Al>
    requires (Cap <= Capacity && Al <= Alignment && (C || !Copyable))
    constexpr function(similar<C, Cap, Al>&& other) noexcept: _invoke{other._invoke}, _vt{other._vt} { relocate(other); }

    constexpr function(const function& other) requires Copyable { copy_from(other); }
    function(const function&) requires (!Copyable) = delete;

    template<size_t Cap, size_t Al>
    requires (Cap <= Capacity && Al <= Alignment)
    constexpr function(const similar<true, Cap, Al>& other) { copy_from(other); }

    constexpr function& operator=(function&& other) noexcept
    {
        replace(std::move(other));
        return *this;
    }

    template<bool C, size_t Cap, size_t Al>
    requires (Cap <= Capacity && Al <= Alignment && (C || !Copyable))
    constexpr function& operator=(similar<C, Cap, Al>&& other) noexcept
    {
        replace(std::move(other));
        return *this;
    }

    constexpr function& operator=(const function& other) requires Copyable
    {
        replace(other);
        return *this;
    }
    function& operator=(const function&) requires (!Copyable) = delete;

    template<size_t Cap, size_t Al>
    requires (Cap <= Capacity && Al <= Alignment)
    constexpr function& operator=(const similar<true, Cap, Al>& other)
    {
        replace(other);
        return *this;
    }

    constexpr function& operator=(std::nullptr_t) noexcept
    {
        destroy();
        clear();
        return *this;
    }

    template<typename F>
    requires fits<F, function, callable<std::decay_t<F>>, Copyable, Noexcept, R, Capacity, Alignment, Args...>
    constexpr function& operator=(F&& f) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F>)
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

    constexpr ~function() noexcept { _vt->destroy(storage(*this)); }

    constexpr void swap(function& other) noexcept
    {
        if (this != &other)
        {
            alignas(local_alignment()) function tmp{std::move(other)};
            other.take(*this);
            take(tmp);
        }
    }

    friend constexpr void swap(function& a, function& b) noexcept { a.swap(b); }

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

    friend constexpr bool operator==(const function& f, std::nullptr_t) noexcept { return f.empty(); }
};

} // namespace floormat::detail::inplace_function_impl

#undef FM_INPLACE_FUNCTION_QUALIFIERS
