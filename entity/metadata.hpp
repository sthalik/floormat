#pragma once
#include "name-of.hpp"
#include "accessor.hpp"
#include "constraints.hpp"
#include "util.hpp"
#include "concepts.hpp"
#include "compat/defs.hpp"
#include <type_traits>
#include <concepts>
#include <utility>
#include <limits>
#include <tuple>
#include <array>
#include <compat/function2.hpp>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Move.h>

namespace floormat::entities {

template<typename T> struct entity_accessors;

} // namespace floormat::entities

namespace floormat::entities::detail {

template<typename F, typename Tuple, size_t N>
requires std::invocable<F, decltype(std::get<N>(std::declval<Tuple>()))>
constexpr CORRADE_ALWAYS_INLINE void visit_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::remove_cvref_t<Tuple>>;
    static_assert(N < Size());

    fun(std::get<N>(tuple));
    if constexpr(N+1 < Size())
        visit_tuple<F, Tuple, N+1>(Corrade::Utility::forward<F>(fun), Corrade::Utility::forward<Tuple>(tuple));
}

template<typename F, typename Tuple, size_t N>
requires std::is_invocable_r_v<bool, F, decltype(std::get<N>(std::declval<Tuple>()))>
constexpr CORRADE_ALWAYS_INLINE bool find_in_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::remove_cvref_t<Tuple>>;
    static_assert(N < Size());

    if (fun(std::get<N>(tuple)))
        return true;
    if constexpr(N+1 < Size())
        return find_in_tuple<F, Tuple, N+1>(Corrade::Utility::forward<F>(fun), Corrade::Utility::forward<Tuple>(tuple));
    return false;
}

template<typename T> struct decay_tuple_;
template<typename... Ts> struct decay_tuple_<std::tuple<Ts...>> { using type = std::tuple<std::decay_t<Ts>...>; };
template<typename T> using decay_tuple = typename decay_tuple_<T>::type;

template<typename Obj, typename Type, typename Default, size_t I, typename... Fs> struct find_reader;

template<typename Obj, typename Type, typename Default, size_t I> struct find_reader<Obj, Type, Default, I> {
    using type = Default;
    static constexpr size_t index = I;
};

template<typename Obj, typename Type, typename Default, size_t I, typename F, typename... Fs>
struct find_reader<Obj, Type, Default, I, F, Fs...> {
    using type = typename find_reader<Obj, Type, Default, I+1, Fs...>::type;
    static constexpr size_t index = find_reader<Obj, Type, Default, I+1, Fs...>::index;
};

template<typename Obj, typename Type, typename Default, size_t I, typename F, typename... Fs>
requires FieldReader<F, Obj, Type>
struct find_reader<Obj, Type, Default, I, F, Fs...> { using type = F; static constexpr size_t index = I; };

} // namespace floormat::entities::detail

namespace floormat::entities {

constexpr auto constantly(const auto& x) noexcept {
    return [x]<typename... Ts> (const Ts&...) constexpr -> const auto& { return x; };
}

template<typename Obj, typename Type> struct entity_field_base {};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
struct entity_field : entity_field_base<Obj, Type> {
private:
    static constexpr auto default_predicate = constantly(field_status::enabled);
    static constexpr auto default_c_range   = constantly(constraints::range<Type>{});
    static constexpr auto default_c_length  = constantly(constraints::max_length{size_t(-1)});
    using default_predicate_t = std::decay_t<decltype(default_predicate)>;
    using default_c_range_t   = std::decay_t<decltype(default_c_range)>;
    using default_c_length_t  = std::decay_t<decltype(default_c_length)>;
    using c_predicate = detail::find_reader<Obj, field_status, default_predicate_t, 0, Ts...>;
    using c_range  = detail::find_reader<Obj, constraints::range<Type>, default_c_range_t, 0, Ts...>;
    using c_length = detail::find_reader<Obj, constraints::max_length, default_c_length_t, 0, Ts...>;
    static constexpr size_t good_arguments =
        unsigned(c_predicate::index != sizeof...(Ts)) +
        unsigned(c_range::index != sizeof...(Ts)) +
        unsigned(c_length::index != sizeof...(Ts));
    static_assert(sizeof...(Ts) == good_arguments, "ignored arguments");

public:
    using ObjectType = Obj;
    using FieldType  = Type;
    using Reader     = R;
    using Writer     = W;
    using Predicate  = typename c_predicate::type;
    using Range      = typename c_range::type;
    using Length     = typename c_length::type;

    StringView name;
    [[no_unique_address]] R reader;
    [[no_unique_address]] W writer;
    [[no_unique_address]] Predicate predicate;
    [[no_unique_address]] Range range;
    [[no_unique_address]] Length length;

    fm_DECLARE_DEFAULT_MOVE_COPY_ASSIGNMENTS(entity_field);

    static constexpr decltype(auto) read(const R& reader, const Obj& x) { return detail::read_field<Obj, Type, R>::read(x, reader); }
    static constexpr void write(const W& writer, Obj& x, Type v);
    constexpr decltype(auto) read(const Obj& x) const { return read(reader, x); }
    constexpr void write(Obj& x, Type value) const { write(writer, x, Corrade::Utility::move(value)); }
    static constexpr bool can_write = !std::is_same_v<std::nullptr_t, decltype(entity_field<Obj, Type, R, W, Ts...>::writer)>;

    static constexpr field_status is_enabled(const Predicate & p, const Obj& x);
    constexpr field_status is_enabled(const Obj& x) const { return is_enabled(predicate, x); }

    static constexpr constraints::range<Type> get_range(const Range& r, const Obj& x);
    constexpr constraints::range<Type> get_range(const Obj& x) const { return get_range(range, x); }
    static constexpr constraints::max_length get_max_length(const Length& l, const Obj& x);
    constexpr constraints::max_length get_max_length(const Obj& x) const { return get_max_length(length, x); }

    constexpr entity_field(StringView name, R r, W w, Ts&&... ts) noexcept :
        name{name}, reader{r}, writer{w},
        predicate { std::get<c_predicate::index>(std::forward_as_tuple(ts..., default_predicate)) },
        range     { std::get<c_range::index>    (std::forward_as_tuple(ts..., default_c_range))   },
        length    { std::get<c_length::index>   (std::forward_as_tuple(ts..., default_c_length))  }
    {}
    constexpr erased_accessor erased() const;
};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr void entity_field<Obj, Type, R, W, Ts...>::write(const W& writer, Obj& x, Type v)
{
    static_assert(can_write); detail::write_field<Obj, Type, W>::write(x, writer, Corrade::Utility::move(v));
}

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr erased_accessor entity_field<Obj, Type, R, W, Ts...>::erased() const
{
    using reader_t    = typename erased_accessor::reader_t;
    using writer_t    = typename erased_accessor::writer_t;
    using predicate_t = typename erased_accessor::predicate_t;
    using c_range_t   = typename erased_accessor::c_range_t;
    using c_length_t  = typename erased_accessor::c_length_t;
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
        Type value_ = Corrade::Utility::move(*reinterpret_cast<Type*>(value));
        write(writer_, obj_, Corrade::Utility::move(value_));
    };
    constexpr auto predicate_fn = [](const void* obj, const predicate_t* predicate) {
        const auto& obj_ = *reinterpret_cast<const Obj*>(obj);
        const auto& predicate_ = *reinterpret_cast<const Predicate*>(predicate);
        return is_enabled(predicate_, obj_);
    };
    constexpr auto writer_stub_fn = [](void*, const writer_t*, void*) {
        fm_abort("no writer for this accessor");
    };
    constexpr bool has_writer = !std::is_same_v<std::decay_t<decltype(writer)>, std::nullptr_t>;

    constexpr auto c_range_fn = [](const void* obj, const c_range_t* reader) -> erased_constraints::range {
        return get_range(*reinterpret_cast<const Range*>(reader), *reinterpret_cast<const Obj*>(obj));
    };
    constexpr auto c_length_fn = [](const void* obj, const c_length_t* reader) -> erased_constraints::max_length {
        return get_max_length(*reinterpret_cast<const Length*>(reader), *reinterpret_cast<const Obj*>(obj));
    };
    return erased_accessor {
        (void*)&reader, has_writer ? (void*)&writer : nullptr,
        (void*)&predicate,
        (void*)&range, (void*)&length,
        name, obj_name, field_name,
        reader_fn, has_writer ? writer_fn : writer_stub_fn,
        predicate_fn,
        c_range_fn, c_length_fn,
    };
}

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr field_status entity_field<Obj, Type, R, W, Ts...>::is_enabled(const Predicate& p, const Obj& x)
{ return detail::read_field<Obj, field_status, Predicate>::read(x, p); }

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr constraints::range<Type> entity_field<Obj, Type, R, W, Ts...>::get_range(const Range& r, const Obj& x)
{ return detail::read_field<Obj, constraints::range<Type>, Range>::read(x, r); }

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr constraints::max_length entity_field<Obj, Type, R, W, Ts...>::get_max_length(const Length& l, const Obj& x)
{ return detail::read_field<Obj, constraints::max_length , Length>::read(x, l); }

template<typename Obj>
struct Entity final {
    static_assert(std::is_same_v<Obj, std::decay_t<Obj>>);

    template<typename Type>
    struct type final
    {
        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
        struct field final : entity_field<Obj, Type, R, W, Ts...>
        {
            constexpr field(StringView field_name, R r, W w, Ts&&... ts) noexcept :
                entity_field<Obj, Type, R, W, Ts...>{field_name, r, w, Corrade::Utility::forward<Ts>(ts)...}
            {}
        };

        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
        field(StringView name, R r, W w, Ts&&... ts) -> field<R, W, Ts...>;
    };
};

template<typename F, typename Tuple>
constexpr void visit_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::decay_t<Tuple>>;
    if constexpr(Size() > 0)
        detail::visit_tuple<F, Tuple, 0>(Corrade::Utility::forward<F>(fun), Corrade::Utility::forward<Tuple>(tuple));
}

template<typename F, typename Tuple>
constexpr bool find_in_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::decay_t<Tuple>>;
    if constexpr(Size() > 0)
        return detail::find_in_tuple<F, Tuple, 0>(Corrade::Utility::forward<F>(fun), Corrade::Utility::forward<Tuple>(tuple));
    else
        return false;
}

} // namespace floormat::entities

namespace floormat {

template<typename T>
class entity_metadata final {
    static_assert(std::is_same_v<T, std::decay_t<T>>);

    template<typename Tuple, std::size_t... Ns>
    static consteval auto erased_helper(const Tuple& tuple, std::index_sequence<Ns...>);

public:
    static constexpr StringView class_name = name_of<T>;
    static constexpr auto accessors = entities::entity_accessors<T>::accessors();
    static constexpr size_t size = std::tuple_size_v<std::decay_t<decltype(accessors)>>;
    static constexpr auto erased_accessors = erased_helper(accessors, std::make_index_sequence<size>{});
};

template<typename T>
template<typename Tuple, std::size_t... Ns>
consteval auto entity_metadata<T>::erased_helper(const Tuple& tuple, std::index_sequence<Ns...>)
{
    std::array<entities::erased_accessor, sizeof...(Ns)> array { std::get<Ns>(tuple).erased()..., };
    return array;
}

} // namespace floormat
