#pragma once
#include "compat/integer-types.hpp"
#include <Corrade/Containers/StringView.h>
#include <concepts>
#include <compare>
#include <type_traits>

namespace floormat {}

namespace floormat::entities {

template<typename T> struct pass_by_value : std::bool_constant<std::is_fundamental_v<T>> {};
template<> struct pass_by_value<StringView> : std::true_type {};
template<typename T> constexpr inline bool pass_by_value_v = pass_by_value<T>::value;

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

} // namespace detail

template<typename Obj>
requires std::is_same_v<Obj, std::decay_t<Obj>>
struct Entity final {
    template<typename Type>
    requires std::is_same_v<Type, std::decay_t<Type>>
    struct type final
    {
        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W>
        struct field final
        {
            using ObjectType = Obj;
            using FieldType = Type;
            using Reader = R;
            using Writer = W;

            StringView name;
            [[no_unique_address]] Reader reader;
            [[no_unique_address]] Writer writer;

            constexpr field(const field&) = default;
            constexpr field& operator=(const field&) = default;
            constexpr decltype(auto) read(const Obj& x) const { return detail::read_field<Obj, Type, R>::read(x, reader); }
            constexpr void write(Obj& x, move_qualified<Type> v) const { detail::write_field<Obj, Type, W>::write(x, writer, v); }

            constexpr field(StringView name, Reader r, Writer w) noexcept : name{name}, reader{r}, writer{w} {}
        };

        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W>
        field(StringView name, R r, W w) -> field<R, W>;
    };
};

enum class erased_field_type : std::uint32_t {
    none,
    string,
    u8, u16, u32, u64, s8, s16, s32, s64,
    user_type_start,
    MAX = (std::uint32_t)-1,
};

template<erased_field_type> struct type_of_erased_field;
template<typename T> struct erased_field_type_v_ : std::integral_constant<erased_field_type, erased_field_type::none> {};

#define FM_ERASED_FIELD_TYPE(TYPE, ENUM)                                                                                    \
    template<> struct erased_field_type_v_<TYPE> : std::integral_constant<erased_field_type, erased_field_type::ENUM> {};   \
    template<> struct type_of_erased_field<erased_field_type::ENUM> { using type = TYPE; }
FM_ERASED_FIELD_TYPE(std::uint8_t, u8);
FM_ERASED_FIELD_TYPE(std::uint16_t, u16);
FM_ERASED_FIELD_TYPE(std::uint32_t, u32);
FM_ERASED_FIELD_TYPE(std::uint64_t, u64);
FM_ERASED_FIELD_TYPE(std::int8_t, s8);
FM_ERASED_FIELD_TYPE(std::int16_t, s16);
FM_ERASED_FIELD_TYPE(std::int32_t, s32);
FM_ERASED_FIELD_TYPE(std::int64_t, s64);
FM_ERASED_FIELD_TYPE(StringView, string);
#undef FM_ERASED_FIELD_TYPE

} // namespace floormat::entities
