#include "split-string.hpp"
#include "compat/assert.hpp"
#include <cr/ArrayView.h>

namespace floormat {

split_string_view::iterator::iterator() noexcept = default;
StringView split_string_view::iterator::operator*() const noexcept { return _cur; }
split_string_view::iterator& split_string_view::iterator::operator++() noexcept { scan(); return *this; }
split_string_view::iterator split_string_view::iterator::operator++(int) noexcept { auto ret = *this; scan(); return ret; }
bool split_string_view::iterator::operator==(const iterator& other) const noexcept { return _cur.data() == other._cur.data(); }
split_string_view::iterator split_string_view::begin() const noexcept { return iterator{*this}; }
split_string_view::iterator split_string_view::end() const noexcept { return {}; }

split_string_view::iterator::iterator(const split_string_view& range) noexcept:
    _rest{range.str}, _delims{range.delims}, _list{range.delim_list},
    _delim{range.delim}, _mode{range.mode}, _keep_empty{range.keep_empty}
{
    // Narrow to the cheapest search giving the same answer, in that order. findOr(char) reaches
    // the SIMD-dispatched stringFindCharacter, while findAnyOr() is a memchr call per input byte
    // and findOr(StringView) a memcmp per position.
    if (_mode == split_mode::any_substring && _list.size() == 1)
    {
        _delims = _list[0];
        _mode = split_mode::substring;
    }
    if ((_mode == split_mode::any_character || _mode == split_mode::substring) && _delims.size() == 1)
    {
        _delim = _delims[0];
        _mode = split_mode::character;
    }
    // Corrade's split() yields nothing for an empty input rather than one empty token.
    if (!range.str.isEmpty())
        scan();
}

StringView split_string_view::iterator::find_delimiter() const noexcept
{
    const char* const end = _rest.end();
    switch (_mode)
    {
    case split_mode::character:
        return _rest.findOr(_delim, end);
    case split_mode::any_character:
        return _rest.findAnyOr(_delims, end);
    case split_mode::substring:
        // stringFindString() reports an empty needle as a match at every position. Treat it as
        // no delimiter at all, like the empty character set, which also keeps a zero-size result
        // unambiguously "not found".
        return _delims.isEmpty() ? StringView{end, 0} : _rest.findOr(_delims, end);
    case split_mode::any_substring:
        break;
    }

    StringView best{end, 0};
    for (StringView needle : _list)
    {
        if (needle.isEmpty())
            continue;
        const auto found = _rest.findOr(needle, end);
        if (found.isEmpty())
            continue;
        // Longest on a tie, so a list containing both "\n" and "\r\n" consumes "\r\n" whole.
        if (found.data() < best.data() || (found.data() == best.data() && found.size() > best.size()))
            best = found;
    }
    return best;
}

void split_string_view::iterator::scan() noexcept
{
    do
    {
        if (!_rest.data())
        {
            _cur = {};
            return;
        }
        const auto delim = find_delimiter();
        _cur = _rest.prefix(delim.data());
        // Null _rest means exhausted, zero-size _rest with non-null data means a trailing
        // empty token is still owed. "a," yields two parts, so emptiness can't end the scan.
        _rest = delim.isEmpty() ? StringView{} : _rest.suffix(delim.end());
    }
    while (!_keep_empty && _cur.isEmpty());
}

split_string_view split_string(StringView str, char delim) noexcept
{ return { .str = str, .delim = delim, .mode = split_mode::character, .keep_empty = true }; }

split_string_view split_string_without_empty_parts(StringView str, char delim) noexcept
{ return { .str = str, .delim = delim, .mode = split_mode::character, .keep_empty = false }; }

split_string_view split_string_on_any(StringView str, StringView delims) noexcept
{ return { .str = str, .delims = delims, .mode = split_mode::any_character, .keep_empty = true }; }

split_string_view split_string_on_any_without_empty_parts(StringView str, StringView delims) noexcept
{ return { .str = str, .delims = delims, .mode = split_mode::any_character, .keep_empty = false }; }

split_string_view split_string_on_substring(StringView str, StringView delim) noexcept
{ return { .str = str, .delims = delim, .mode = split_mode::substring, .keep_empty = true }; }

split_string_view split_string_on_substring_without_empty_parts(StringView str, StringView delim) noexcept
{ return { .str = str, .delims = delim, .mode = split_mode::substring, .keep_empty = false }; }

split_string_view split_string_on_any_substring(StringView str, ArrayView<const StringView> delims) noexcept
{ return { .str = str, .delim_list = delims, .mode = split_mode::any_substring, .keep_empty = true }; }

split_string_view split_string_on_any_substring_without_empty_parts(StringView str, ArrayView<const StringView> delims) noexcept
{ return { .str = str, .delim_list = delims, .mode = split_mode::any_substring, .keep_empty = false }; }

split_string_view split_string_on_any_substring(StringView str, std::initializer_list<StringView> delims) noexcept
{ return split_string_on_any_substring(str, ArrayView<const StringView>{delims.begin(), delims.size()}); }

split_string_view split_string_on_any_substring_without_empty_parts(StringView str, std::initializer_list<StringView> delims) noexcept
{ return split_string_on_any_substring_without_empty_parts(str, ArrayView<const StringView>{delims.begin(), delims.size()}); }

ArrayView<StringView> split_string_into(ArrayView<StringView> out, split_string_view range)
{
    uint32_t i = 0;
    for (StringView token : range)
    {
        fm_assert(i < out.size());
        out[i++] = token;
    }
    return out.prefix(i);
}

// _parts is default-initialized first by declaration order, then written through here.
split_array::split_array(split_string_view range):
    _size{(uint32_t)split_string_into({_parts, max_size}, range).size()}
{}

} // namespace floormat
