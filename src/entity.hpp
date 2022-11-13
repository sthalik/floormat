#pragma once
#include "compat/integer-types.hpp"
#include <concepts>
#include <type_traits>
#include <tuple>
#include <utility>
#include <algorithm>

#include <Corrade/Containers/StringView.h>

namespace floormat {}

namespace floormat::entities {

template<typename T> struct pass_by_value : std::bool_constant<std::is_fundamental_v<T>> {};
template<typename T> constexpr inline bool pass_by_value_v = pass_by_value<T>::value;
template<> struct pass_by_value<StringView> : std::true_type {};

template<typename T> using const_qualified = std::conditional_t<pass_by_value_v<T>, T, const T&>;
template<typename T> using ref_qualified = std::conditional_t<pass_by_value_v<T>, T, T&>;
template<typename T> using move_qualified = std::conditional_t<pass_by_value_v<T>, T, T&&>;

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
             FieldWriter_function<F, T, FieldType>;
};

template<typename Obj, typename Type, typename R>
struct read_field {
    static Type read(const Obj& x, R r) { return r(x); }
};

template<typename Obj, typename Type>
struct read_field<Obj, Type, Type (Obj::*)() const> {
    static Type read(const Obj& x, Type (Obj::*r)() const) { return (x.*r)(); }
};

template<typename Obj, typename Type>
struct read_field<Obj, Type, Type Obj::*> {
    static Type read(const Obj& x, Type Obj::*r) { return x.*r; }
};

template<typename Obj, typename FieldType, typename Writer> struct write_field {
    static void write(Obj& x, Writer w, move_qualified<FieldType> value) { w(x, value); }
};

template<typename Obj, typename Type>
struct write_field<Obj, Type, void(Obj::*)(move_qualified<Type>)> {
    static void write(Obj& x, void(Obj::*w)(move_qualified<Type>), move_qualified<Type> value) { (x.*w)(value); }
};

template<typename Obj, typename Type>
struct write_field<Obj, Type, Type Obj::*> {
    static void write(Obj& x, Type Obj::* w, move_qualified<Type> value) { x.*w = value; }
};

template<typename Obj, typename FieldType, FieldReader<Obj, FieldType> R, FieldWriter<Obj, FieldType> W>
struct field {
    using Object = Obj;
    using Type = FieldType;
    using Reader = R;
    using Writer = W;

    StringView name;
    [[no_unique_address]] Reader reader;
    [[no_unique_address]] Writer writer;

    constexpr field(StringView name, Reader r, Writer w) noexcept : name{name}, reader{r}, writer{w} {}
    decltype(auto) read(const Obj& x) const { return read_field<Obj, FieldType, R>::read(x, reader); }
    void write(Obj& x, move_qualified<FieldType> v) const { write_field<Obj, FieldType, W>::write(x, writer, v); }
};

template<typename Obj>
struct entity final {
    template<typename FieldType>
    struct Field {
        template<FieldReader<Obj, FieldType> R, FieldWriter<Obj, FieldType> W>
        static consteval auto make(StringView name, R r, W w) noexcept {
            return field<Obj, FieldType, R, W> { name, r, w };
        }
    };
};

template<typename Key, typename KeyT, typename... Xs>
struct assoc final {
    template<typename T> using Types = std::tuple<Xs...>;
    consteval assoc(Xs&&... xs) {

    }

private:
    template<typename T> struct cell { Key key; T value; };
    std::tuple<cell<Xs>...> _tuple;
};

} // namespace floormat::entities
