#pragma once
#include "binary-serializer.hpp"
#include <iterator>
#include <Corrade/Containers/StringView.h>

namespace floormat::Serialize {

template<std::output_iterator<char> It>
struct binary_writer final {
    explicit constexpr binary_writer(It it, size_t allocated_bytes) noexcept;
    template<serializable T> constexpr void write(T x) noexcept;
    constexpr void write_asciiz_string(StringView str) noexcept;
    constexpr size_t bytes_written() const noexcept { return _bytes_written; }
    constexpr size_t bytes_allocated() const noexcept { return _bytes_allocated; }

private:
    It it;
    size_t _bytes_written, _bytes_allocated;
};

template<std::output_iterator<char> It, serializable T>
constexpr CORRADE_ALWAYS_INLINE binary_writer<It>& operator<<(binary_writer<It>& writer, T x) noexcept;

} // namespace floormat::Serialize
