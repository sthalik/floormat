#pragma once
#include "compat/integer-types.hpp"
#include <type_traits>
#include <concepts>
#include <functional>

#include <Corrade/Containers/StringView.h>

namespace floormat {}

namespace floormat::entities {

template<typename T> using const_qualified = std::conditional_t<std::is_fundamental_v<T>, T, const T&>;
template<typename T> using ref_qualified = std::conditional_t<std::is_fundamental_v<T>, T, T&>;
template<typename T> using move_qualified = std::conditional_t<std::is_fundamental_v<T>, T, T&&>;

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
    static FieldType write(const Obj& x, Writer w, move_qualified<FieldType> value) { return w(x, value); }
};

template<typename Obj, typename Type>
struct write_field<Obj, Type, void(Type::*)(move_qualified<Type>)> {
    static Type write(const Obj& x, void(Type::*w)(Type&&), move_qualified<Type> value) { return (x.*w)(value); }
};

template<typename Obj, typename Type>
struct write_field<Obj, Type, Type Obj::*> {
    static Type write(const Obj& x, Type Obj::* w, move_qualified<Type> value) { return x.*w = value; }
};

template<typename Obj, typename FieldType, FieldReader<Obj, FieldType> R, FieldWriter<Obj, FieldType> W>
struct field {
    using Object = Obj;
    using Type = FieldType;
    using Reader = R;
    using Writer = W;

    StringView name;
    Reader reader;
    Writer writer;

    constexpr field(StringView name, Reader r, Writer w) noexcept : name{name}, reader{r}, writer{w} {}
    decltype(auto) read(const Obj& x) const { return read_field<Obj, FieldType, R>::read(x, reader); }
    void write(Obj& x, move_qualified<FieldType> v) const { return write_field<Obj, FieldType, R>::write(x, v); }
};

template<typename Obj>
struct entity {
    template<typename FieldType>
    struct Field {
        template<FieldReader<Obj, FieldType> R, FieldWriter<Obj, FieldType> W>
        struct make final : field<Obj, FieldType, R, W> {
            consteval make(StringView name_, R r, W w) noexcept : field<Obj, FieldType, R, W>{name_, r, w} {}
        };
        template<FieldReader<Obj, FieldType> R, FieldWriter<Obj, FieldType> W>
        make(StringView name, R r, W w) -> make<R, W>;
    };
};

} // namespace floormat::entities
