#pragma once
#include "name-of.hpp"
#include "accessor.hpp"
#include "field.hpp"
#include "concepts.hpp"
#include <utility>
#include <array>
#include <compat/function2.hpp>
#include <Corrade/Containers/StringView.h>

namespace floormat::entities::detail {

template<typename F, typename Tuple, size_t N>
requires std::invocable<F, decltype(std::get<N>(std::declval<Tuple>()))>
constexpr CORRADE_ALWAYS_INLINE void visit_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::remove_cvref_t<Tuple>>;
    static_assert(N < Size());

    fun(std::get<N>(tuple));
    if constexpr(N+1 < Size())
        visit_tuple<F, Tuple, N+1>(floormat::forward<F>(fun), floormat::forward<Tuple>(tuple));
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
        return find_in_tuple<F, Tuple, N+1>(floormat::forward<F>(fun), floormat::forward<Tuple>(tuple));
    return false;
}

template<typename T> struct decay_tuple_;
template<typename... Ts> struct decay_tuple_<std::tuple<Ts...>> { using type = std::tuple<std::decay_t<Ts>...>; };
template<typename T> using decay_tuple = typename decay_tuple_<T>::type;

} // namespace floormat::entities::detail

namespace floormat::entities {

template<typename F, typename Tuple>
constexpr void visit_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::decay_t<Tuple>>;
    if constexpr(Size() > 0)
        detail::visit_tuple<F, Tuple, 0>(floormat::forward<F>(fun), floormat::forward<Tuple>(tuple));
}

template<typename F, typename Tuple>
constexpr bool find_in_tuple(F&& fun, Tuple&& tuple)
{
    using Size = std::tuple_size<std::decay_t<Tuple>>;
    if constexpr(Size() > 0)
        return detail::find_in_tuple<F, Tuple, 0>(floormat::forward<F>(fun), floormat::forward<Tuple>(tuple));
    else
        return false;
}

struct inspect_intent_t   {};
struct serialize_intent_t {};
struct report_intent_t    {};

template<typename T, typename Intent> struct entity_accessors;

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
                entity_field<Obj, Type, R, W, Ts...>{field_name, r, w, floormat::forward<Ts>(ts)...}
            {}
        };

        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
        field(StringView name, R r, W w, Ts&&... ts) -> field<R, W, Ts...>;
    };
};

constexpr inline auto ignored_write = []<typename O, typename T>(O&, T) {};

} // namespace floormat::entities

namespace floormat {

template<typename T, typename Intent>
class entity_metadata final {
    static_assert(std::is_same_v<T, std::decay_t<T>>);

    template<typename Tuple, std::size_t... Ns>
    static consteval auto erased_helper(const Tuple& tuple, std::index_sequence<Ns...>);

public:
    static constexpr StringView class_name = name_of<T>;
    static constexpr auto accessors = entities::entity_accessors<T, Intent>::accessors();
    static constexpr size_t size = std::tuple_size_v<std::decay_t<decltype(accessors)>>;
    static constexpr auto erased_accessors = erased_helper(accessors, std::make_index_sequence<size>{});

    entity_metadata() = default;
};

template<typename T, typename Intent>
template<typename Tuple, std::size_t... Ns>
consteval auto entity_metadata<T, Intent>::erased_helper(const Tuple& tuple, std::index_sequence<Ns...>)
{
    std::array<entities::erased_accessor, sizeof...(Ns)> array { std::get<Ns>(tuple).erased()..., };
    return array;
}

} // namespace floormat
