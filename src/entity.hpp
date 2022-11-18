#pragma once
#include "compat/integer-types.hpp"
#include <concepts>
#include <compare>
#include <type_traits>
#include <utility>
#include <tuple>
#include <typeinfo>
#include <array>
#include <compat/function2.hpp>
#include <Corrade/Containers/StringView.h>

#if defined _MSC_VER
#define FM_PRETTY_FUNCTION __FUNCSIG__
#else
#define FM_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

namespace floormat::entities::detail {
template<typename T> static constexpr StringView type_name();
template<typename T> struct type_name_helper;
}

namespace floormat {
template<typename T> constexpr inline StringView name_of = entities::detail::type_name_helper<T>::name;
} // namespace floormat

namespace floormat::entities {

template<typename T, typename = void> struct pass_by_value : std::bool_constant<std::is_fundamental_v<T>> {};
template<> struct pass_by_value<StringView> : std::true_type {};
template<typename T> struct pass_by_value<T, std::enable_if_t<std::is_trivially_copy_constructible_v<T> && sizeof(T) <= sizeof(void*)>> : std::true_type {};
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

template<typename T>
static constexpr StringView type_name() {
    using namespace Corrade::Containers;
    using SVF = StringViewFlag;
    constexpr const char* str = FM_PRETTY_FUNCTION;
    return StringView { str, Implementation::strlen_(str), SVF::Global|SVF::NullTerminated };
}

template<typename T>
struct type_name_helper final
{
    static constexpr const StringView name = type_name<T>();
};

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

template<typename Obj, typename Type, bool IsOwning, bool IsCopyable, typename Capacity, bool IsThrowing, bool HasStrongExceptionGuarantee>
struct read_field<Obj, Type, fu2::function_base<IsOwning, IsCopyable, Capacity, IsThrowing, HasStrongExceptionGuarantee, void(const Obj&, move_qualified<Type>) const>> {
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

template<typename Obj, typename Type, bool IsOwning, bool IsCopyable, typename Capacity, bool IsThrowing, bool HasStrongExceptionGuarantee>
struct write_field<Obj, Type, fu2::function_base<IsOwning, IsCopyable, Capacity, IsThrowing, HasStrongExceptionGuarantee, void(Obj&, move_qualified<Type>) const>> {
    template<typename F> static constexpr void write(Obj& x, F&& fun, move_qualified<Type> value) { fun(x, value); }
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
template<typename... Ts> struct decay_tuple_<std::tuple<Ts...>> {
    using type = std::tuple<std::decay_t<Ts>...>;
};

template<typename T>
using decay_tuple = typename decay_tuple_<T>::type;

template<typename T>
struct accessors_for_
{
    using type = decay_tuple<std::decay_t<decltype(T::accessors())>>;
};

template<typename T>
using accessors_for = typename accessors_for_<T>::type;

} // namespace detail

struct erased_accessor final {
    using erased_reader_t = void;
    using erased_writer_t = void;
    using Object = void;
    using Value = void;

    const erased_reader_t* reader;
    const erased_writer_t* writer;
    StringView object_name, field_type_name;
    void(*read_fun)(const Object*, const erased_reader_t*, Value*);
    void(*write_fun)(Object*, const erased_writer_t*, Value*);

    constexpr erased_accessor(const erased_accessor&) = default;
    constexpr erased_accessor(erased_reader_t* reader, erased_writer_t * writer,
                               StringView object_name, StringView field_type_name,
                               void(*read_fun)(const Object*, const erased_reader_t*, Value*),
                               void(*write_fun)(Object*, const erased_writer_t*, Value*)) :
        reader{reader}, writer{writer},
        object_name{object_name}, field_type_name{field_type_name},
        read_fun{read_fun}, write_fun{write_fun}
    {}

    template<typename T, typename FieldType>
    static constexpr bool check_name_static()
    {
        return !std::is_pointer_v<T> && !std::is_reference_v<T> &&
               !std::is_pointer_v<FieldType> && !std::is_reference_v<T>;
    }

    template<typename T, typename FieldType>
    constexpr bool check_name() const noexcept
    {
        static_assert(check_name_static<T, FieldType>());
        constexpr auto obj = name_of<T>, field = name_of<FieldType>;
        return (obj.data() == object_name.data() && field.data() == field_type_name.data()) ||
               obj == object_name && field == field_type_name;
    }

    template<typename Obj, typename FieldType>
    constexpr void assert_name() const noexcept
    {
        fm_assert(check_name<Obj, FieldType>());
    }

    template<typename Obj, typename FieldType>
    void read_unchecked(const Obj& x, FieldType& value) const noexcept
    {
        static_assert(check_name_static<Obj, FieldType>());
        read_fun(&x, reader, &value);
    }

    template<typename Obj, typename FieldType>
    requires std::is_default_constructible_v<FieldType>
    FieldType read_unchecked(const Obj& x) const noexcept
    {
        static_assert(check_name_static<Obj, FieldType>());
        FieldType value;
        read_unchecked(x, value);
        return value;
    }

    template<typename Obj, typename FieldType>
    void write_unchecked(Obj& x, move_qualified<FieldType> value) const noexcept
    {
        static_assert(check_name_static<Obj, FieldType>());
        write_fun(&x, writer, &value);
    }

    template<typename Obj, typename FieldType>
    requires std::is_default_constructible_v<FieldType>
    FieldType read(const Obj& x) const noexcept { assert_name<Obj, FieldType>(); return read_unchecked<Obj, FieldType>(x); }

    template<typename Obj, typename FieldType>
    void read(const Obj& x, FieldType& value) const noexcept { assert_name<Obj, FieldType>(); read_unchecked<Obj, FieldType>(x, value); }

    template<typename Obj, typename FieldType>
    void write(Obj& x, move_qualified<FieldType> value) const noexcept { assert_name<Obj, FieldType>(); write_unchecked<Obj, FieldType>(x, value); }
};

template<typename Obj, typename Type> struct entity_field_base {};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W>
struct entity_field : entity_field_base<Obj, Type> {
    using ObjectType = Obj;
    using FieldType = Type;
    using Reader = R;
    using Writer = W;

    StringView name;
    [[no_unique_address]] R reader;
    [[no_unique_address]] W writer;

    constexpr entity_field(const entity_field&) = default;
    constexpr entity_field& operator=(const entity_field&) = default;
    static constexpr decltype(auto) read(const R& reader, const Obj& x) { return detail::read_field<Obj, Type, R>::read(x, reader); }
    static constexpr void write(const W& writer, Obj& x, move_qualified<Type> v) { detail::write_field<Obj, Type, W>::write(x, writer, v); }
    constexpr decltype(auto) read(const Obj& x) const { return read(reader, x); }
    constexpr void write(Obj& x, move_qualified<Type> value) const { write(writer, x, value); }
    constexpr entity_field(StringView name, R r, W w) noexcept : name{name}, reader{r}, writer{w} {}

    constexpr erased_accessor erased() const {
        using reader_t = typename erased_accessor::erased_reader_t;
        using writer_t = typename erased_accessor::erased_writer_t;
        constexpr auto obj_name = name_of<Obj>, field_name = name_of<Type>;

        constexpr auto reader_fn = [](const void* obj, const reader_t* reader, void* value)
        {
            const auto& obj_ = *reinterpret_cast<const Obj*>(obj);
            const auto& reader_ = *reinterpret_cast<const R*>(reader);
            auto& value_ = *reinterpret_cast<Type*>(value);
            value_ = read(reader_, obj_);
        };
        constexpr auto writer_fn = [](void* obj, const writer_t* writer, void* value)
        {
            auto& obj_ = *reinterpret_cast<Obj*>(obj);
            const auto& writer_ = *reinterpret_cast<const W*>(writer);
            move_qualified<Type> value_ = std::move(*reinterpret_cast<Type*>(value));
            write(writer_, obj_, value_);
        };
        return erased_accessor{
            (void*)&reader, (void*)&writer,
            obj_name, field_name,
            reader_fn, writer_fn,
        };
    }
};

template<typename Obj>
struct Entity final {
    static_assert(std::is_same_v<Obj, std::decay_t<Obj>>);

    template<typename Type>
    struct type final
    {
        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W>
        struct field final : entity_field<Obj, Type, R, W>
        {
            constexpr field(StringView field_name, R r, W w) noexcept :
                entity_field<Obj, Type, R, W>{field_name, r, w}
            {}
        };

        template<FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W>
        field(StringView name, R r, W w) -> field<R, W>;
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

enum class erased_field_type : std::uint32_t {
    none,
    string,
    u8, u16, u32, u64, s8, s16, s32, s64,
    user_type_start,
    MAX = (1u << 31) - 1u,
    DYNAMIC = (std::uint32_t)-1,
};

template<erased_field_type> struct type_of_erased_field;
template<typename T> struct erased_field_type_v_ : std::integral_constant<erased_field_type, erased_field_type::DYNAMIC> {};

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

namespace floormat {

template<typename T>
requires std::is_same_v<T, std::decay_t<T>>
class entity_metadata final {
    template<typename... Ts>
    static constexpr auto erased_helper(const std::tuple<Ts...>& tuple)
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
