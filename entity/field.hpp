#pragma once

#include "constraints.hpp"
#include "concepts.hpp"
#include "accessor.hpp"
#include "field-status.hpp"
#include "compat/defs.hpp"

namespace floormat::entities::detail {
template<typename Obj, typename Type, typename Default, size_t I, typename... Fs> struct find_reader;

template<typename Obj, typename Type, typename Default, size_t I> struct find_reader<Obj, Type, Default, I> {
    using type = Default;
    static constexpr size_t index = I;
};

template<typename Obj, typename Type, typename Default, size_t I, typename F, typename... Fs>
struct find_reader<Obj, Type, Default, I, F, Fs...> {
    using type = typename find_reader<Obj, Type, Default, I+1, Fs...>::type;
    static constexpr size_t index = find_reader<Obj, Type, Default, I+1, Fs...>::index;
};

template<typename Obj, typename Type, typename Default, size_t I, typename F, typename... Fs>
requires FieldReader<F, Obj, Type>
struct find_reader<Obj, Type, Default, I, F, Fs...> { using type = F; static constexpr size_t index = I; };

} // namespace floormat::entities::detail

namespace floormat::entities {

constexpr auto constantly(const auto& x) noexcept {
    return [x]<typename... Ts> (const Ts&...) constexpr -> const auto& { return x; };
}

template<typename Obj, typename Type> struct entity_field_base {};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
struct entity_field : entity_field_base<Obj, Type> {
private:
    static constexpr auto default_predicate = constantly(field_status::enabled);
    static constexpr auto default_c_range   = constantly(constraints::range<Type>{});
    static constexpr auto default_c_length  = constantly(constraints::max_length{size_t(-1)});
    using default_predicate_t = std::decay_t<decltype(default_predicate)>;
    using default_c_range_t   = std::decay_t<decltype(default_c_range)>;
    using default_c_length_t  = std::decay_t<decltype(default_c_length)>;
    using c_predicate = detail::find_reader<Obj, field_status, default_predicate_t, 0, Ts...>;
    using c_range  = detail::find_reader<Obj, constraints::range<Type>, default_c_range_t, 0, Ts...>;
    using c_length = detail::find_reader<Obj, constraints::max_length, default_c_length_t, 0, Ts...>;
    static constexpr size_t good_arguments =
        unsigned(c_predicate::index != sizeof...(Ts)) +
        unsigned(c_range::index != sizeof...(Ts)) +
        unsigned(c_length::index != sizeof...(Ts));
    static_assert(sizeof...(Ts) == good_arguments, "ignored arguments");

public:
    using ObjectType = Obj;
    using FieldType  = Type;
    using Reader     = R;
    using Writer     = W;
    using Predicate  = typename c_predicate::type;
    using Range      = typename c_range::type;
    using Length     = typename c_length::type;

    StringView name;
    [[fm_no_unique_address]] R reader;
    [[fm_no_unique_address]] W writer;
    [[fm_no_unique_address]] Predicate predicate;
    [[fm_no_unique_address]] Range range;
    [[fm_no_unique_address]] Length length;

    fm_DECLARE_DEFAULT_MOVE_COPY_ASSIGNMENTS(entity_field);

    static constexpr decltype(auto) read(const R& reader, const Obj& x) { return detail::read_field<Obj, Type, R>::read(x, reader); }
    static constexpr void write(const W& writer, Obj& x, Type v);
    constexpr decltype(auto) read(const Obj& x) const { return read(reader, x); }
    constexpr void write(Obj& x, Type value) const { write(writer, x, move(value)); }
    static constexpr bool can_write = !std::is_same_v<std::nullptr_t, decltype(entity_field<Obj, Type, R, W, Ts...>::writer)>;

    static constexpr field_status is_enabled(const Predicate & p, const Obj& x);
    constexpr field_status is_enabled(const Obj& x) const { return is_enabled(predicate, x); }

    static constexpr constraints::range<Type> get_range(const Range& r, const Obj& x);
    constexpr constraints::range<Type> get_range(const Obj& x) const { return get_range(range, x); }
    static constexpr constraints::max_length get_max_length(const Length& l, const Obj& x);
    constexpr constraints::max_length get_max_length(const Obj& x) const { return get_max_length(length, x); }

    constexpr entity_field(StringView name, R r, W w, Ts&&... ts) noexcept :
        name{name}, reader{r}, writer{w},
        predicate { std::get<c_predicate::index>(std::forward_as_tuple(ts..., default_predicate)) },
        range     { std::get<c_range::index>    (std::forward_as_tuple(ts..., default_c_range))   },
        length    { std::get<c_length::index>   (std::forward_as_tuple(ts..., default_c_length))  }
    {}
    constexpr erased_accessor erased() const;
};

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr void entity_field<Obj, Type, R, W, Ts...>::write(const W& writer, Obj& x, Type v)
{
    static_assert(can_write); detail::write_field<Obj, Type, W>::write(x, writer, move(v));
}

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr erased_accessor entity_field<Obj, Type, R, W, Ts...>::erased() const
{
    using reader_t    = typename erased_accessor::reader_t;
    using writer_t    = typename erased_accessor::writer_t;
    using predicate_t = typename erased_accessor::predicate_t;
    using c_range_t   = typename erased_accessor::c_range_t;
    using c_length_t  = typename erased_accessor::c_length_t;
    constexpr auto obj_name = name_of<Obj>, field_name = name_of<Type>;

    constexpr auto reader_fn = [](const void* obj, const reader_t* reader, void* value) {
        const auto& obj_ = *static_cast<const Obj*>(obj);
        const auto& reader_ = *static_cast<const R*>(reader);
        auto& value_ = *static_cast<Type*>(value);
        value_ = read(reader_, obj_);
    };
    constexpr auto writer_fn = [](void* obj, const writer_t* writer, void* value) {
        auto& obj_ = *static_cast<Obj*>(obj);
        const auto& writer_ = *static_cast<const W*>(writer);
        Type value_ = move(*static_cast<Type*>(value));
        write(writer_, obj_, move(value_));
    };
    constexpr auto predicate_fn = [](const void* obj, const predicate_t* predicate) {
        const auto& obj_ = *static_cast<const Obj*>(obj);
        const auto& predicate_ = *static_cast<const Predicate*>(predicate);
        return is_enabled(predicate_, obj_);
    };
    constexpr auto writer_stub_fn = [](void*, const writer_t*, void*) {
        fm_abort("no writer for this accessor");
    };
    constexpr bool has_writer = !std::is_same_v<std::decay_t<decltype(writer)>, std::nullptr_t>;

    constexpr auto c_range_fn = [](const void* obj, const c_range_t* reader) -> erased_constraints::range {
        return get_range(*static_cast<const Range*>(reader), *static_cast<const Obj*>(obj));
    };
    constexpr auto c_length_fn = [](const void* obj, const c_length_t* reader) -> erased_constraints::max_length {
        return get_max_length(*static_cast<const Length*>(reader), *static_cast<const Obj*>(obj));
    };
    return erased_accessor {
        (void*)&reader, has_writer ? (void*)&writer : nullptr,
        (void*)&predicate,
        (void*)&range, (void*)&length,
        name, obj_name, field_name,
        reader_fn, has_writer ? writer_fn : writer_stub_fn,
        predicate_fn,
        c_range_fn, c_length_fn,
    };
}

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr field_status entity_field<Obj, Type, R, W, Ts...>::is_enabled(const Predicate& p, const Obj& x)
{ return detail::read_field<Obj, field_status, Predicate>::read(x, p); }

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr constraints::range<Type> entity_field<Obj, Type, R, W, Ts...>::get_range(const Range& r, const Obj& x)
{ return detail::read_field<Obj, constraints::range<Type>, Range>::read(x, r); }

template<typename Obj, typename Type, FieldReader<Obj, Type> R, FieldWriter<Obj, Type> W, typename... Ts>
constexpr constraints::max_length entity_field<Obj, Type, R, W, Ts...>::get_max_length(const Length& l, const Obj& x)
{ return detail::read_field<Obj, constraints::max_length , Length>::read(x, l); }

} // namespace floormat::entities
