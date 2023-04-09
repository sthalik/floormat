#pragma once
#include "util.hpp"
#include "compat/function2.hpp"
#include "compat/assert.hpp"
#include <concepts>
#include <type_traits>

namespace floormat::entities {

template<typename F, typename T, typename FieldType>
concept FieldReader_memfn = requires(const T& x, F f) {
    { (x.*f)() } -> std::convertible_to<FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldReader_ptr = requires(const T& x, F f) {
    { x.*f } -> std::convertible_to<FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldReader_function = requires(const T& x, F f) {
    { f(x) } -> std::convertible_to<FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldReader = requires {
    requires FieldReader_memfn<F, T, FieldType> ||
             FieldReader_ptr<F, T, FieldType> ||
             FieldReader_function<F, T, FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldWriter_memfn = requires(T& x, move_qualified<FieldType> value, F f) {
    { (x.*f)(value) } -> std::same_as<void>;
};

template<typename F, typename T, typename FieldType>
concept FieldWriter_ptr = requires(T& x, move_qualified<FieldType> value, F f) {
    { x.*f = value };
};

template<typename F, typename T, typename FieldType>
concept FieldWriter_function = requires(T& x, move_qualified<FieldType> value, F f) {
    { f(x, value) } -> std::same_as<void>;
};

template<typename F, typename T, typename FieldType>
concept FieldWriter = requires {
    requires FieldWriter_memfn<F, T, FieldType> ||
             FieldWriter_ptr<F, T, FieldType> ||
             FieldWriter_function<F, T, FieldType> ||
             std::same_as<F, std::nullptr_t>;
};

} // namespace floormat::entities

namespace floormat::entities::detail {

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

} // namespace floormat::entities::detail
