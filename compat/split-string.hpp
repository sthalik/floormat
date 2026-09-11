#pragma once
#include <cr/StringView.h>
#include <cr/ArrayView.h>
#include <initializer_list>
#include <iterator>

// A braced delimiter list outlives the call only under P2718R0, which extends every temporary
// in a range-initializer to the end of the loop. GCC 15, Clang 19, MSVC 19.51.
#if __cpp_range_based_for < 202211L
#error "split_string_on_any_substring() needs P2718R0 range-for temporary lifetime extension"
#endif

namespace floormat {

enum class split_mode : uint8_t
{
    character,      // delim
    any_character,  // any one char of delims
    substring,      // delims as a whole
    any_substring,  // any one entry of delim_list, as a whole
};

struct split_string_view
{
    class iterator;

    StringView str;
    StringView delims = {}; // char set, or the whole delimiter
    ArrayView<const StringView> delim_list = {};
    char delim = 0;
    split_mode mode = split_mode::character;
    bool keep_empty = false;

    iterator begin() const noexcept;
    iterator end() const noexcept;
};

class split_string_view::iterator
{
    StringView _rest, _cur, _delims;
    ArrayView<const StringView> _list;
    char _delim = 0;
    split_mode _mode = split_mode::character;
    bool _keep_empty = false;

    friend struct split_string_view;
    explicit iterator(const split_string_view& range) noexcept;
    StringView find_delimiter() const noexcept;
    void scan() noexcept;

public:
    // operator*() yields a prvalue. C++20 forward iterators allow that, the C++17 ones
    // require value_type&, so the two tags have to disagree. Same split as std::ranges::iota_view.
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type = StringView;
    using difference_type = ptrdiff_t;

    iterator() noexcept;
    StringView operator*() const noexcept;
    iterator& operator++() noexcept;
    iterator operator++(int) noexcept;
    // Equality means position, and only iterators from the same split_view are comparable.
    bool operator==(const iterator& other) const noexcept;
};

split_string_view split_string(StringView str, char delim) noexcept;
split_string_view split_string_without_empty_parts(StringView str, char delim) noexcept;
split_string_view split_string_on_any(StringView str, StringView delims) noexcept;
split_string_view split_string_on_any_without_empty_parts(StringView str, StringView delims) noexcept;
split_string_view split_string_on_substring(StringView str, StringView delim) noexcept;
split_string_view split_string_on_substring_without_empty_parts(StringView str, StringView delim) noexcept;
// Leftmost match wins, longest on a tie, so {"\n", "\r\n"} behaves the same either way round.
split_string_view split_string_on_any_substring(StringView str, ArrayView<const StringView> delims) noexcept;
split_string_view split_string_on_any_substring_without_empty_parts(StringView str, ArrayView<const StringView> delims) noexcept;
// The braced list dies at the end of the full expression, so the result can be iterated or
// copied out there and not stored. Take the ArrayView overload for anything longer-lived.
split_string_view split_string_on_any_substring(StringView str, std::initializer_list<StringView> delims) noexcept;
split_string_view split_string_on_any_substring_without_empty_parts(StringView str, std::initializer_list<StringView> delims) noexcept;

ArrayView<StringView> split_string_into(ArrayView<StringView> out, split_string_view range);

// Eager split into inline storage, for callers that want a sized contiguous range
// rather than a rescanning forward one. More than max_size parts aborts.
class split_array
{
public:
    static constexpr uint32_t max_size = 16;

private:
    StringView _parts[max_size];
    uint32_t _size = 0;

public:
    split_array() noexcept = default;
    explicit split_array(split_string_view range);

    uint32_t size() const noexcept { return _size; }
    bool isEmpty() const noexcept { return _size == 0; }
    const StringView* data() const noexcept { return _parts; }
    const StringView* begin() const noexcept { return _parts; }
    const StringView* end() const noexcept { return _parts + _size; }
    operator ArrayView<const StringView>() const noexcept { return {_parts, _size}; }
};

} // namespace floormat
