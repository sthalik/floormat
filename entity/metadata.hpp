#pragma once
#include "name-of.hpp"
#include "accessor.hpp"
#include "util.hpp"
#include <cstddef>
#include <concepts>
#include <type_traits>
#include <utility>
#include <tuple>
#include <array>
#include <compat/function2.hpp>
#include <Corrade/Containers/StringView.h>

namespace floormat::entities {

template<typename F, typename T, typename FieldType>
concept FieldReader_memfn = requires(const T x, F f) {
    { (x.*f)() } -> std::convertible_to<FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldReader_ptr = requires(const T x, F f) {
    { x.*f } -> std::convertible_to<FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldReader_function = requires(const T x, F f) {
    { f(x) } -> std::convertible_to<FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldReader = requires {
    requires FieldReader_memfn<F, T, FieldType> ||
             FieldReader_ptr<F, T, FieldType> ||
             FieldReader_function<F, T, FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldWriter_memfn = requires(T x, move_qualified<FieldType> value, F f) {
    { (x.*f)(value) } -> std::same_as<void>;
};

template<typename F, typename T, typename FieldType>
concept FieldWriter_ptr = requires(T x, move_qualified<FieldType> value, F f) {
    { x.*f = value };
};

template<typename F, typename T, typename FieldType>
concept FieldWriter_function = requires(T x, move_qualified<FieldType> value, F f) {
    { f(x, value) } -> std::same_as<void>;
};

template<typename F, typename T, typename FieldType>
concept FieldWriter = requires {
    requires FieldWriter_memfn<F, T, FieldType> ||
             FieldWriter_ptr<F, T, FieldType> ||
             FieldWriter_function<F, T, FieldType> ||
             std::same_as<F, std::nullptr_t>;
};

template<typename F, typename T>
concept FieldPredicate = requires {
    requires FieldReader<F, T, bool> || std::same_as<F, std::nullptr_t>;
};

namespace detail {

template<typename Obj, typename Type, FieldReader<Obj, Type> R>
struct read_field {
    static constexpr Type read(const Obj& x, R r) { return r(x); }
};

template<typename Obj, typename Type>
struct read_field<Obj, Type, Type (Obj::*)() const> {
    static constexpr Type read(const Obj& x, Type (Obj::*r)() const) { return (x.*r)(); }
};

template<typename Obj, typename Type>
struct read_field<Obj, Type, Type Obj::*> {
    static constexpr Type read(const Obj& x, Type Obj::*r) { return x.*r; }
};

template<typename Obj, typename Type, bool Owning, bool Copyable, typename Capacity, bool Throwing, bool ExcGuarantee>
struct read_field<Obj, Type, fu2::function_base<Owning, Copyable, Capacity, Throwing, ExcGuarantee, void(const Obj&, move_qualified<Type>) const>>
{
    template<typename F> static constexpr Type read(const Obj& x, F&& fun) { return fun(x); }
};

template<typename Obj, typename FieldType, FieldWriter<Obj, FieldType> W> struct write_field {
    static constexpr void write(Obj& x, W w, move_qualified<FieldType> value) { w(x, value); }
};

template<typename Obj, typename FieldType>
struct write_field<Obj, FieldType, void(Obj::*)(move_qualified<FieldType>)> {
    static constexpr void write(Obj& x, void(Obj::*w)(move_qualified<FieldType>), move_qualified<FieldType> value) { (x.*w)(value); }
};

template<typename Obj, typename FieldType>
struct write_field<Obj, FieldType, FieldType Obj::*> {
    static constexpr void write(Obj& x, FieldType Obj::* w, move_qualified<FieldType> value) { x.*w = value; }
};

template<typename Obj, typename Type, bool Owning, bool Copyable, typename Capacity, bool Throwing, bool ExcGuarantee>
struct write_field<Obj, Type, fu2::function_base<Owning, Copyable, Capacity, Throwing, ExcGuarantee, void(Obj&, move_qualified<Type>) const>> {
    template<typename F> static constexpr void write(Obj& x, F&& fun, move_qualified<Type> value) { fun(x, value); }
};

template<typename Obj, typename FieldType>
struct write_field<Obj, FieldType, std::nullptr_t> {
    static constexpr void write(Obj&, std::nullptr_t, move_qualified<FieldType>) { fm_abort("no writing for this accessor"); }
};

template<typename F, typename Tuple, std::size_t N>
requires std::invocable<F, decltype(std::get<N>(std::declval<Tuple>()))>
constexpr CORRADE_ALWAYS_INLINE void visit_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::remove_cvref_t<Tuple>>;
    static_assert(N < Size());

    fun(std::get<N>(tuple));
    if constexpr(N+1 < Size())
        visit_tuple<F, Tuple, N+1>(std::forward<F>(fun), std::forward<Tuple>(tuple));
}

template<typename F, typename Tuple, std::size_t N>
requires std::is_invocable_r_v<bool, F, decltype(std::get<N>(std::declval<Tuple>()))>
constexpr CORRADE_ALWAYS_INLINE bool find_in_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::remove_cvref_t<Tuple>>;
    static_assert(N < Size());

    if (fun(std::get<N>(tuple)))
        return true;
    if constexpr(N+1 < Size())
        return find_in_tuple<F, Tuple, N+1>(std::forward<F>(fun), std::forward<Tuple>(tuple));
    return false;
}

template<typename T> struct decay_tuple_;
template<typename... Ts> struct decay_tuple_<std::tuple<Ts...>> { using type = std::tuple<std::decay_t<Ts>...>; };
template<typename T> using decay_tuple = typename decay_tuple_<T>::type;
template<typename T> struct accessors_for_ { using type = decay_tuple<std::decay_t<decltype(T::accessors())>>; };
template<typename T> using accessors_for = typename accessors_for_<T>::type;

} // namespace detail

template<typename Obj, typename Type> struct entity_field_base {};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, FieldPredicate<Obj> P = std::nullptr_t>
struct entity_field : entity_field_base<Obj, Type> {
    using ObjectType = Obj;
    using FieldType = Type;
    using Reader = R;
    using Writer = W;
    using Predicate = P;

    StringView name;
    [[no_unique_address]] R reader;
    [[no_unique_address]] W writer;
    [[no_unique_address]] P predicate;

    constexpr entity_field(const entity_field&) = default;
    constexpr entity_field& operator=(const entity_field&) = default;
    static constexpr decltype(auto) read(const R& reader, const Obj& x) { return detail::read_field<Obj, Type, R>::read(x, reader); }
    static constexpr void write(const W& writer, Obj& x, move_qualified<Type> v);
    constexpr decltype(auto) read(const Obj& x) const { return read(reader, x); }
    constexpr void write(Obj& x, move_qualified<Type> value) const { write(writer, x, value); }
    static constexpr bool can_write = !std::is_same_v<std::nullptr_t, decltype(entity_field<Obj, Type, R, W, P>::writer)>;
    static constexpr bool is_enabled(const P& p, const Obj& x);
    constexpr entity_field(StringView name, R r, W w, P p = nullptr) noexcept : name{name}, reader{r}, writer{w}, predicate{p} {}
    constexpr erased_accessor erased() const;
};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, FieldPredicate<Obj> P>
constexpr void entity_field<Obj, Type, R, W, P>::write(const W& writer, Obj& x, move_qualified<Type> v)
{
    static_assert(can_write); detail::write_field<Obj, Type, W>::write(x, writer, v);
}

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, FieldPredicate<Obj> P>
constexpr erased_accessor entity_field<Obj, Type, R, W, P>::erased() const
{
    using reader_t = typename erased_accessor::erased_reader_t;
    using writer_t = typename erased_accessor::erased_writer_t;
    using predicate_t = typename erased_accessor::erased_predicate_t ;
    constexpr auto obj_name = name_of<Obj>, field_name = name_of<Type>;

    constexpr auto reader_fn = [](const void* obj, const reader_t* reader, void* value) {
        const auto& obj_ = *reinterpret_cast<const Obj*>(obj);
        const auto& reader_ = *reinterpret_cast<const R*>(reader);
        auto& value_ = *reinterpret_cast<Type*>(value);
        value_ = read(reader_, obj_);
    };
    constexpr auto writer_fn = [](void* obj, const writer_t* writer, void* value) {
        auto& obj_ = *reinterpret_cast<Obj*>(obj);
        const auto& writer_ = *reinterpret_cast<const W*>(writer);
        move_qualified<Type> value_ = std::move(*reinterpret_cast<Type*>(value));
        write(writer_, obj_, value_);
    };
    constexpr auto predicate_fn = [](const void* obj, const predicate_t* predicate) {
        const auto& obj_ = *reinterpret_cast<const Obj*>(obj);
        const auto& predicate_ = *reinterpret_cast<const P*>(predicate);
        return is_enabled(predicate_, obj_);
    };
    constexpr auto predicate_stub_fn = [](const void*, const predicate_t*) {
        return true;
    };
    constexpr auto writer_stub_fn = [](void*, const writer_t*, void*) {
        fm_abort("no writer for this accessor");
    };

    return erased_accessor {
        (void*)&reader, writer ? (void*)&writer : nullptr,
        std::is_same_v<P, std::nullptr_t> ? (const void*)&predicate : nullptr,
        name, obj_name, field_name,
        reader_fn, writer ? writer_fn : writer_stub_fn,
        std::is_same_v<P, std::nullptr_t> ? predicate_fn : predicate_stub_fn,
    };
}

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, FieldPredicate<Obj> P>
constexpr bool entity_field<Obj, Type, R, W, P>::is_enabled(const P& p, const Obj& x)
{
    if constexpr(std::is_same_v<P, std::nullptr_t>)
        return true;
    else
        return detail::read_field<Obj, Type, P>::read(x, p);
}

template<typename Obj>
struct Entity final {
    static_assert(std::is_same_v<Obj, std::decay_t<Obj>>);

    template<typename Type>
    struct type final
    {
        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, FieldPredicate<Obj> P = std::nullptr_t>
        struct field final : entity_field<Obj, Type, R, W, P>
        {
            constexpr field(StringView field_name, R r, W w, P p = nullptr) noexcept :
                entity_field<Obj, Type, R, W, P>{field_name, r, w, p}
            {}
        };

        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, FieldPredicate<Obj> P = std::nullptr_t>
        field(StringView name, R r, W w, P p = nullptr) -> field<R, W, P>;
    };
};

template<typename F, typename Tuple>
constexpr void visit_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::decay_t<Tuple>>;
    if constexpr(Size() > 0)
        detail::visit_tuple<F, Tuple, 0>(std::forward<F>(fun), std::forward<Tuple>(tuple));
}

template<typename F, typename Tuple>
constexpr bool find_in_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::decay_t<Tuple>>;
    if constexpr(Size() > 0)
        return detail::find_in_tuple<F, Tuple, 0>(std::forward<F>(fun), std::forward<Tuple>(tuple));
    else
        return false;
}

} // namespace floormat::entities

namespace floormat {

template<typename T>
requires std::is_same_v<T, std::decay_t<T>>
class entity_metadata final {
    template<typename... Ts>
    static consteval auto erased_helper(const std::tuple<Ts...>& tuple)
    {
        std::array<entities::erased_accessor, sizeof...(Ts)> array { std::get<Ts>(tuple).erased()..., };
        return array;
    }
public:
    static constexpr StringView class_name = name_of<T>;
    static constexpr std::size_t size = std::tuple_size_v<entities::detail::accessors_for<T>>;
    static constexpr entities::detail::accessors_for<T> accessors = T::accessors();
    static constexpr auto erased_accessors = erased_helper(accessors);
};

} // namespace floormat
