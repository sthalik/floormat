#pragma once
#include "util.hpp"
#include <type_traits>
#include <Corrade/Containers/StringView.h>

namespace floormat::entities {

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
    static constexpr bool check_name_static();

    template<typename T, typename FieldType>
    constexpr bool check_name() const noexcept;

    template<typename Obj, typename FieldType> constexpr void assert_name() const noexcept;
    template<typename Obj, typename FieldType> void read_unchecked(const Obj& x, FieldType& value) const noexcept;
    template<typename Obj, typename FieldType> requires std::is_default_constructible_v<FieldType> FieldType read_unchecked(const Obj& x) const noexcept;
    template<typename Obj, typename FieldType> void write_unchecked(Obj& x, move_qualified<FieldType> value) const noexcept;
    template<typename Obj, typename FieldType> requires std::is_default_constructible_v<FieldType> FieldType read(const Obj& x) const noexcept;
    template<typename Obj, typename FieldType> void read(const Obj& x, FieldType& value) const noexcept;
    template<typename Obj, typename FieldType> void write(Obj& x, move_qualified<FieldType> value) const noexcept;
    constexpr bool can_write() const noexcept { return writer != nullptr; }
};

template<typename T, typename FieldType>
constexpr bool erased_accessor::check_name_static()
{
    return !std::is_pointer_v<T> && !std::is_reference_v<T> &&
           !std::is_pointer_v<FieldType> && !std::is_reference_v<T>;
}

template<typename T, typename FieldType>
constexpr bool erased_accessor::check_name() const noexcept
{
    static_assert(check_name_static<T, FieldType>());
    constexpr auto obj = name_of<T>, field = name_of<FieldType>;
    return (obj.data() == object_name.data() && field.data() == field_type_name.data()) ||
           obj == object_name && field == field_type_name;
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

} // namespace floormat::entities
