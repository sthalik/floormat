#pragma once
#include <cstdint>
#include <type_traits>

namespace Corrade::Containers {
template<typename T> class BasicStringView;
using StringView = BasicStringView<const char>;
} // namespace Corrade::Containers

namespace floormat::entities {

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
