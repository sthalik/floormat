#pragma once
#include "util.hpp"
#include "erased-constraints.hpp"
#include <type_traits>
#include <utility>
#include <Corrade/Containers/StringView.h>

namespace floormat::erased_constraints {

struct range;
struct max_length;
struct group;

} // namespace floormat::erased_constraints

namespace floormat::entities {

enum class field_status : unsigned char { enabled, hidden, readonly, };

struct erased_accessor final {
    using reader_t = void;
    using writer_t = void;
    using predicate_t = void;
    using c_range_t = void;
    using c_length_t = void;
    using c_group_t = void;
    using Object = void;
    using Value = void;

    const reader_t* reader;
    const writer_t* writer;
    const predicate_t* predicate;
    const c_range_t* range;
    const c_length_t* length;
    const c_group_t* group;

    StringView field_name, object_type, field_type;
    void(*read_fun)(const Object*, const reader_t*, Value*);
    void(*write_fun)(Object*, const writer_t*, Value*);
    field_status(*predicate_fun)(const Object*, const predicate_t*);
    erased_constraints::range(*range_fun)(const Object*, const c_range_t*);
    erased_constraints::max_length(*length_fun)(const Object*, const c_length_t*);
    erased_constraints::group(*group_fun)(const Object*, const c_group_t*);

    constexpr erased_accessor(const erased_accessor&) = default;
    constexpr erased_accessor(const reader_t* reader, const writer_t* writer, const predicate_t* predicate,
                              const c_range_t* range, const c_length_t* length, const c_group_t* group,
                              StringView field_name, StringView object_name, StringView field_type_name,
                              void(*read_fun)(const Object*, const reader_t*, Value*),
                              void(*write_fun)(Object*, const writer_t*, Value*),
                              field_status(*predicate_fun)(const Object*, const predicate_t*),
                              erased_constraints::range(*range_fun)(const Object*, const c_range_t*),
                              erased_constraints::max_length(*length_fun)(const Object*, const c_length_t*),
                              erased_constraints::group(*group_fun)(const Object*, const c_group_t*)) :
        reader{reader}, writer{writer}, predicate{predicate},
        range{range}, length{length}, group{group},
        field_name{field_name}, object_type{object_name}, field_type{field_type_name},
        read_fun{read_fun}, write_fun{write_fun}, predicate_fun{predicate_fun},
        range_fun{range_fun}, length_fun{length_fun}, group_fun{group_fun}
    {}

    template<typename T, typename FieldType>
    static constexpr bool check_name_static();

    template<typename T, typename FieldType>
    constexpr bool check_name() const noexcept;

    template<typename Obj>
    constexpr void do_asserts() const;

    template<typename Obj, typename FieldType> constexpr void assert_name() const noexcept;
    template<typename Obj, typename FieldType> void read_unchecked(const Obj& x, FieldType& value) const noexcept;
    template<typename Obj, typename FieldType> requires std::is_default_constructible_v<FieldType> FieldType read_unchecked(const Obj& x) const noexcept;
    template<typename Obj, typename FieldType> void write_unchecked(Obj& x, move_qualified<FieldType> value) const noexcept;
    template<typename Obj, typename FieldType> requires std::is_default_constructible_v<FieldType> FieldType read(const Obj& x) const noexcept;
    template<typename Obj, typename FieldType> void read(const Obj& x, FieldType& value) const noexcept;
    template<typename Obj, typename FieldType> void write(Obj& x, move_qualified<FieldType> value) const noexcept;

    template<typename Obj> field_status is_enabled(const Obj& x) const noexcept;
    constexpr bool can_write() const noexcept { return writer != nullptr; }
    template<typename Obj> erased_constraints::range get_range(const Obj& x) const noexcept;
    template<typename Obj> erased_constraints::max_length get_max_length(const Obj& x) const noexcept;
    template<typename Obj> erased_constraints::group get_group(const Obj& x) const noexcept;
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

template<typename Obj>
field_status erased_accessor::is_enabled(const Obj& x) const noexcept
{
    do_asserts<Obj>();
    return predicate_fun(&x, predicate);
}

template<typename T> erased_constraints::range erased_accessor::get_range(const T& x) const noexcept { do_asserts<T>();return range_fun(&x,range); }
template<typename T> erased_constraints::max_length erased_accessor::get_max_length(const T& x) const noexcept { do_asserts<T>();return length_fun(&x,length); }
template<typename T> erased_constraints::group erased_accessor::get_group(const T& x) const noexcept { do_asserts<T>();return group_fun(&x, group); }

} // namespace floormat::entities
