#pragma once
#include "name-of.hpp"
#include "accessor.hpp"
#include "constraints.hpp"
#include "util.hpp"
#include "concepts.hpp"
#include <cstddef>
#include <type_traits>
#include <limits>
#include <utility>
#include <tuple>
#include <array>
#include <compat/function2.hpp>
#include <Corrade/Containers/StringView.h>

namespace floormat::entities {

namespace detail {

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

template<typename Obj, typename Type, typename Default, std::size_t I, typename... Fs> struct find_reader_;

template<typename Obj, typename Type, typename Default, std::size_t I> struct find_reader_<Obj, Type, Default, I> {
    using type = Default;
    static constexpr std::size_t index = I;
};

template<typename Obj, typename Type, typename Default, std::size_t I, typename F, typename... Fs>
struct find_reader_<Obj, Type, Default, I, F, Fs...> {
    using type = typename find_reader_<Obj, Type, Default, I+1, Fs...>::type;
    static constexpr std::size_t index = find_reader_<Obj, Type, Default, I+1, Fs...>::index;
};

template<typename Obj, typename Type, typename Default, std::size_t I, typename F, typename... Fs>
requires FieldReader<F, Obj, Type>
struct find_reader_<Obj, Type, Default, I, F, Fs...> { using type = F; static constexpr std::size_t index = I; };

template<typename Obj, typename Type, typename Default, typename... Fs>
using find_reader = typename find_reader_<Obj, Type, Default, 0, std::decay_t<Fs>...>::type;

template<typename Obj, typename Type, typename Default, typename... Fs>
constexpr std::size_t find_reader_index = find_reader_<Obj, Type, Default, 0, std::decay_t<Fs>...>::index;

template<typename Obj, auto constant>
constexpr auto constantly = [](const Obj&) constexpr { return constant; };


} // namespace detail

template<typename Obj, typename Type> struct entity_field_base {};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
struct entity_field : entity_field_base<Obj, Type> {
private:
    static constexpr auto default_predicate = detail::constantly<Obj, field_status::enabled>;
    using default_predicate_t = std::decay_t<decltype(default_predicate)>;
public:
    using ObjectType = Obj;
    using FieldType = Type;
    using Reader = R;
    using Writer = W;
    using Predicate = std::decay_t<detail::find_reader<Obj, field_status, default_predicate_t, Ts...>>;

    StringView name;
    [[no_unique_address]] R reader;
    [[no_unique_address]] W writer;
    [[no_unique_address]] Predicate predicate;

    constexpr entity_field(const entity_field&) = default;
    constexpr entity_field& operator=(const entity_field&) = default;
    static constexpr decltype(auto) read(const R& reader, const Obj& x) { return detail::read_field<Obj, Type, R>::read(x, reader); }
    static constexpr void write(const W& writer, Obj& x, move_qualified<Type> v);
    constexpr decltype(auto) read(const Obj& x) const { return read(reader, x); }
    constexpr void write(Obj& x, move_qualified<Type> value) const { write(writer, x, value); }
    static constexpr bool can_write = !std::is_same_v<std::nullptr_t, decltype(entity_field<Obj, Type, R, W, Ts...>::writer)>;
    static constexpr field_status is_enabled(const Predicate & p, const Obj& x);
    constexpr entity_field(StringView name, R r, W w, Ts&&... ts) noexcept :
        name{name}, reader{r}, writer{w},
        predicate{std::get<detail::find_reader_index<Obj, field_status, default_predicate_t, Ts...>>(std::forward_as_tuple(ts..., default_predicate))}
    {}
    constexpr erased_accessor erased() const;
};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr void entity_field<Obj, Type, R, W, Ts...>::write(const W& writer, Obj& x, move_qualified<Type> v)
{
    static_assert(can_write); detail::write_field<Obj, Type, W>::write(x, writer, v);
}

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr erased_accessor entity_field<Obj, Type, R, W, Ts...>::erased() const
{
    using reader_t = typename erased_accessor::erased_reader_t;
    using writer_t = typename erased_accessor::erased_writer_t;
    using predicate_t = typename erased_accessor::erased_predicate_t ;
    constexpr auto obj_name = name_of<Obj>, field_name = name_of<Type>;
    using P = Predicate;

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
    constexpr auto writer_stub_fn = [](void*, const writer_t*, void*) {
        fm_abort("no writer for this accessor");
    };

    return erased_accessor {
        (void*)&reader, writer ? (void*)&writer : nullptr, (void*)&predicate,
        name, obj_name, field_name,
        reader_fn, writer ? writer_fn : writer_stub_fn, predicate_fn,
    };
}

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr field_status entity_field<Obj, Type, R, W, Ts...>::is_enabled(const Predicate& p, const Obj& x)
{
    return detail::read_field<Obj, field_status, Predicate>::read(x, p);
}

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
                entity_field<Obj, Type, R, W, Ts...>{field_name, r, w, std::forward<Ts>(ts)...}
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
