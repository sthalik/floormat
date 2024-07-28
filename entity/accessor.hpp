#pragma once
#include "util.hpp"
#include "erased-constraints.hpp"
#include "name-of.hpp"
#include "field-status.hpp"
#include <Corrade/Containers/StringView.h>

namespace floormat::entities {

struct erased_accessor final {
    using reader_t = void;
    using writer_t = void;
    using predicate_t = void;
    using c_range_t = void;
    using c_length_t = void;
    using Object = void;
    using Value = void;

    const reader_t* reader = nullptr;
    const writer_t* writer = nullptr;
    const predicate_t* predicate = nullptr;
    const c_range_t* range = nullptr;
    const c_length_t* length = nullptr;

    StringView field_name, object_type, field_type;
    void(*read_fun)(const Object*, const reader_t*, Value*) = nullptr;
    void(*write_fun)(Object*, const writer_t*, Value*) = nullptr;
    field_status(*predicate_fun)(const Object*, const predicate_t*) = nullptr;
    erased_constraints::range(*range_fun)(const Object*, const c_range_t*) = nullptr;
    erased_constraints::max_length(*length_fun)(const Object*, const c_length_t*) = nullptr;

    explicit constexpr erased_accessor() noexcept = default;
    constexpr erased_accessor(const erased_accessor&) = default;
    constexpr erased_accessor(const reader_t* reader, const writer_t* writer, const predicate_t* predicate,
                              const c_range_t* range, const c_length_t* length,
                              StringView field_name, StringView object_name, StringView field_type_name,
                              void(*read_fun)(const Object*, const reader_t*, Value*),
                              void(*write_fun)(Object*, const writer_t*, Value*),
                              field_status(*predicate_fun)(const Object*, const predicate_t*),
                              erased_constraints::range(*range_fun)(const Object*, const c_range_t*),
                              erased_constraints::max_length(*length_fun)(const Object*, const c_length_t*)) :
        reader{reader}, writer{writer}, predicate{predicate},
        range{range}, length{length},
        field_name{field_name}, object_type{object_name}, field_type{field_type_name},
        read_fun{read_fun}, write_fun{write_fun}, predicate_fun{predicate_fun},
        range_fun{range_fun}, length_fun{length_fun}
    {}

    template<typename T, typename FieldType>
    [[nodiscard]] static constexpr bool check_name_static();

    template<typename T, typename FieldType>
    [[nodiscard]] constexpr bool check_name() const noexcept;

    template<typename FieldType>
    [[nodiscard]] constexpr bool check_field_type() const noexcept;

    template<typename Obj>
    constexpr void do_asserts() const;

    template<typename Obj, typename FieldType> constexpr void assert_name() const noexcept;
    template<typename Obj, typename FieldType> void read_unchecked(const Obj& x, FieldType& value) const noexcept;
    template<typename Obj, typename FieldType> requires std::is_default_constructible_v<FieldType> FieldType read_unchecked(const Obj& x) const noexcept;
    template<typename Obj, typename FieldType> void write_unchecked(Obj& x, move_qualified<FieldType> value) const noexcept;
    template<typename Obj, typename FieldType> requires std::is_default_constructible_v<FieldType> FieldType read(const Obj& x) const noexcept;
    template<typename Obj, typename FieldType> void read(const Obj& x, FieldType& value) const noexcept;
    template<typename Obj, typename FieldType> void write(Obj& x, move_qualified<FieldType> value) const noexcept;

    inline field_status is_enabled(const void* x) const noexcept;
    inline bool can_write() const noexcept { return writer != nullptr; }
    inline erased_constraints::range get_range(const void* x) const noexcept;
    inline erased_constraints::max_length get_max_length(const void* x) const noexcept;
};

template<typename T, typename FieldType>
constexpr bool erased_accessor::check_name_static()
{
    return !std::is_pointer_v<T> && !std::is_reference_v<T> &&
           !std::is_pointer_v<FieldType> && !std::is_reference_v<T>;
}

template<typename Obj>
constexpr void erased_accessor::do_asserts() const
{
    static_assert(!std::is_pointer_v<Obj> && !std::is_reference_v<Obj>);
    constexpr auto obj = name_of<Obj>;
    fm_assert(obj.data() == object_type.data() || obj == object_type);
}

template<typename T, typename FieldType>
constexpr bool erased_accessor::check_name() const noexcept
{
    static_assert(check_name_static<T, FieldType>());
    constexpr auto obj = name_of<T>, field = name_of<FieldType>;
    return (obj.data() == object_type.data() && field.data() == field_type.data()) ||
           obj == object_type && field == field_type;
}

template<typename FieldType>
constexpr bool erased_accessor::check_field_type() const noexcept
{
    constexpr auto name = name_of<FieldType>;
    return field_type == name;
}

template<typename Obj, typename FieldType>
constexpr void erased_accessor::assert_name() const noexcept
{
    fm_assert(check_name<Obj, FieldType>());
}

template<typename Obj, typename FieldType>
void erased_accessor::read_unchecked(const Obj& x, FieldType& value) const noexcept
{
    static_assert(check_name_static<Obj, FieldType>());
    read_fun(&x, reader, &value);
}

template<typename Obj, typename FieldType>
requires std::is_default_constructible_v<FieldType>
FieldType erased_accessor::read_unchecked(const Obj& x) const noexcept
{
    static_assert(check_name_static<Obj, FieldType>());
    FieldType value;
    read_unchecked(x, value);
    return value;
}

template<typename Obj, typename FieldType>
void erased_accessor::write_unchecked(Obj& x, move_qualified<FieldType> value) const noexcept
{
    static_assert(check_name_static<Obj, FieldType>());
    write_fun(&x, writer, &value);
}

template<typename Obj, typename FieldType>
requires std::is_default_constructible_v<FieldType>
FieldType erased_accessor::read(const Obj& x) const noexcept {
    assert_name<Obj, FieldType>();
    return read_unchecked<Obj, FieldType>(x);
}

template<typename Obj, typename FieldType>
void erased_accessor::read(const Obj& x, FieldType& value) const noexcept
{
    assert_name<Obj, FieldType>();
    read_unchecked<Obj, FieldType>(x, value);
}

template<typename Obj, typename FieldType>
void erased_accessor::write(Obj& x, move_qualified<FieldType> value) const noexcept
{
    assert_name<Obj, FieldType>();
    write_unchecked<Obj, FieldType>(x, value);
}

field_status erased_accessor::is_enabled(const void* x) const noexcept { return predicate_fun(x, predicate); }
erased_constraints::range erased_accessor::get_range(const void* x) const noexcept { return range_fun(x,range); }
erased_constraints::max_length erased_accessor::get_max_length(const void* x) const noexcept { return length_fun(x,length); }

} // namespace floormat::entities
