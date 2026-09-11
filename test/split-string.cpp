#include "app.hpp"
#include "compat/split-string.hpp"
#include "compat/assert.hpp"
#include "compat/array-size.hpp"
#include <algorithm>
#include <ranges>
#include <cr/Array.h>
#include <cr/String.h>
#include <cr/StringIterable.h>

namespace floormat {

namespace {

static_assert(std::forward_iterator<split_string_view::iterator>);
static_assert(std::ranges::forward_range<split_string_view>);
static_assert(std::ranges::contiguous_range<split_array>);
static_assert(std::ranges::sized_range<split_array>);

// Pointers are compared, not just contents: a token that matches by value but points
// elsewhere means the view was rebuilt instead of sliced out of the input.
void compare(split_string_view range, ArrayView<const StringView> expected)
{
    uint32_t i = 0;
    for (StringView token : range)
    {
        fm_assert(i < expected.size());
        fm_assert(token.data() == expected[i].data());
        fm_assert(token.size() == expected[i].size());
        i++;
    }
    fm_assert(i == expected.size());
}

struct part { uint32_t offset, size; };

void compare(split_string_view range, StringView str, ArrayView<const part> expected)
{
    uint32_t i = 0;
    for (StringView token : range)
    {
        fm_assert(i < expected.size());
        fm_assert(token.data() == str.data() + expected[i].offset);
        fm_assert(token.size() == expected[i].size);
        i++;
    }
    fm_assert(i == expected.size());
}

constexpr StringView inputs[] = {
    ""_s, ","_s, "a"_s, "a,b"_s, "a,,b"_s, ",a"_s, "a,"_s, ",,"_s,
    "a;b,c"_s, "  a , b "_s, "abc"_s, ";;a;;"_s,
};

constexpr StringView delimiter_sets[] = { ","_s, ",;"_s, " "_s, " ,;"_s };

// Corrade has no keep-empty split over a character *set*, only over a single char
// (split(char)) or a whole substring (split(StringView)). Multi-char sets are checked
// by hand in test_keep_empty_on_sets().
void test_against_corrade()
{
    for (auto str : inputs)
    {
        for (auto delims : delimiter_sets)
            compare(split_string_on_any_without_empty_parts(str, delims), str.splitOnAnyWithoutEmptyParts(delims));

        compare(split_string(str, ','), str.split(','));
        compare(split_string_without_empty_parts(str, ','), str.splitWithoutEmptyParts(','));
        compare(split_string(str, ';'), str.split(';'));
        compare(split_string_without_empty_parts(str, ';'), str.splitWithoutEmptyParts(';'));

        // Same answers through the set form, which collapses a one-character set to findOr().
        compare(split_string_on_any(str, ","_s), str.split(','));
        compare(split_string_on_any_without_empty_parts(str, ","_s), str.splitWithoutEmptyParts(','));
        compare(split_string_on_any(str, ";"_s), str.split(';'));
        compare(split_string_on_any_without_empty_parts(str, ";"_s), str.splitWithoutEmptyParts(';'));
    }
}

void test_empty_delimiter_set()
{
    // memchr() with a zero-length set never matches, so nothing is a delimiter.
    for (auto str : inputs)
    {
        const StringView whole[] = { str };
        const auto expected = str.isEmpty() ? ArrayView<const StringView>{} : whole;
        compare(split_string_on_any(str, ""_s), expected);
        compare(split_string_on_any_without_empty_parts(str, ""_s), expected);

        // ""_s has non-null data. A null set reaches memchr(nullptr, c, 0).
        compare(split_string_on_any(str, StringView{}), expected);
        compare(split_string_on_any_without_empty_parts(str, StringView{}), expected);
    }
}

void test_keep_empty_on_sets()
{
    constexpr auto str = "a;b,,c"_s;
    constexpr part expected[] = { {0, 1}, {2, 1}, {4, 0}, {5, 1} };
    compare(split_string_on_any(str, ",;"_s), str, expected);

    constexpr auto str2 = ";,"_s;
    constexpr part expected2[] = { {0, 0}, {1, 0}, {2, 0} };
    compare(split_string_on_any(str2, ",;"_s), str2, expected2);
}

void test_iterator()
{
    constexpr auto str = "a,b"_s;
    const auto range = split_string(str, ',');
    const auto end = range.end();
    fm_assert(split_string_view::iterator{} == end);

    auto it = range.begin();
    fm_assert(it != end);
    ++it;
    fm_assert(it != end);
    ++it;
    fm_assert(it == end);

    fm_assert(split_string(""_s, ',').begin() == split_string(""_s, ',').end());

    // begin() rescans, so one split_view value iterates more than once.
    constexpr part expected[] = { {0, 1}, {2, 1} };
    compare(range, str, expected);
    compare(range, str, expected);
}

void test_multipass()
{
    constexpr auto str = "a,bb,ccc"_s;
    const auto range = split_string(str, ',');

    auto it = range.begin();
    const auto copy = it;
    fm_assert(it == copy);
    ++it;
    fm_assert(it != copy);
    // The copy keeps its own token. Without this the iterator would only be input, not forward.
    fm_assert((*copy).data() == str.data() && (*copy).size() == 1);
    fm_assert((*it).data() == str.data() + 2 && (*it).size() == 2);

    auto it2 = range.begin();
    const auto prev = it2++;
    fm_assert((*prev).data() == str.data());
    fm_assert((*it2).data() == str.data() + 2);
    fm_assert(&++it2 == &it2);
}

void test_ranges()
{
    constexpr auto str = "a,bb,,ccc,dddd"_s;
    constexpr StringView expected[] = { "a"_s, "bb"_s, "ccc"_s, "dddd"_s };

    // An rvalue split_view is not a view, so this goes through std::ranges::owning_view.
    fm_assert(std::ranges::distance(split_string_without_empty_parts(str, ',')) == 4);
    fm_assert(std::ranges::equal(split_string_without_empty_parts(str, ','), expected));

    uint32_t n = 0;
    for (StringView token : split_string(str, ',')
                          | std::views::filter([](StringView s) { return s.size() > 1; })
                          | std::views::take(2))
    {
        fm_assert(token.data() >= str.data() && token.data() < str.end());
        n++;
    }
    fm_assert(n == 2);
}

void test_split_array()
{
    constexpr auto str = "a,bb,,ccc"_s;
    const split_array arr{split_string_without_empty_parts(str, ',')};
    fm_assert(arr.size() == 3);
    fm_assert(!arr.isEmpty());
    fm_assert(arr.data() == arr.begin());
    fm_assert(arr.end() - arr.begin() == 3);

    // Indexing is through the ArrayView conversion rather than a member operator[].
    const ArrayView<const StringView> view = arr;
    constexpr part expected[] = { {0, 1}, {2, 2}, {6, 3} };
    for (uint32_t i = 0; i < array_size(expected); i++)
    {
        fm_assert(view[i].data() == str.data() + expected[i].offset);
        fm_assert(view[i].size() == expected[i].size);
    }

    fm_assert(std::ranges::equal(arr, split_string_without_empty_parts(str, ',')));
    fm_assert("/"_s.join(arr) == "a/bb/ccc"_s);

    fm_assert(split_array{}.isEmpty());
    fm_assert(split_array{split_string(""_s, ',')}.isEmpty());
}

void test_split_array_full()
{
    char buf[split_array::max_size * 2];
    uint32_t n = 0;
    for (uint32_t i = 0; i < split_array::max_size; i++)
    {
        if (i > 0)
            buf[n++] = ',';
        buf[n++] = char('a' + i);
    }
    const StringView str{buf, n};
    const split_array arr{split_string(str, ',')};
    fm_assert(arr.size() == split_array::max_size);
    fm_assert(std::ranges::equal(arr, split_string(str, ',')));
}

void test_embedded_nul()
{
    constexpr char raw[] = { 'a', '\0', 'b', ',', 'c' };
    const StringView str{raw, array_size(raw)};

    constexpr part on_comma[] = { {0, 3}, {4, 1} };
    compare(split_string(str, ','), str, on_comma);

    constexpr part on_nul[] = { {0, 1}, {2, 3} };
    compare(split_string(str, '\0'), str, on_nul);
}

void test_non_ascii()
{
    // char is signed here, so a delimiter above 0x7f is the interesting case.
    constexpr char raw[] = { '\xff', 'a', '\xff', 'b', '\x80', 'c' };
    const StringView str{raw, array_size(raw)};
    compare(split_string(str, '\xff'), str.split('\xff'));

    constexpr part on_both[] = { {0, 0}, {1, 1}, {3, 1}, {5, 1} };
    compare(split_string_on_any(str, "\xff\x80"_s), str, on_both);
}

void test_alignment_sweep()
{
    // stringFindCharacter is SIMD-dispatched, so walk one delimiter across every offset
    // and length. An overread past buf shows up as an ASan redzone hit.
    char buf[139];
    for (auto& c : buf)
        c = 'a';
    for (uint32_t len = 1; len <= array_size(buf); len++)
        for (uint32_t at = 0; at < len; at++)
        {
            buf[at] = ',';
            const StringView str{buf, len};
            const part expected[] = { {0, at}, {at + 1, len - at - 1} };
            compare(split_string(str, ','), str, expected);
            buf[at] = 'a';
        }
}

void test_long_input()
{
    char buf[199];
    uint32_t n = 0;
    for (uint32_t i = 0; i < 100; i++)
    {
        if (i > 0)
            buf[n++] = ',';
        buf[n++] = char('a' + i % 26);
    }
    const StringView str{buf, n};
    compare(split_string(str, ','), str.split(','));
    compare(split_string_on_any_without_empty_parts(str, ",;"_s), str.splitOnAnyWithoutEmptyParts(",;"_s));
}

void test_split_into()
{
    StringView buf[8];
    const auto parts = split_string_into(buf, split_string_without_empty_parts("a,,b,c"_s, ','));
    fm_assert(parts.size() == 3);
    fm_assert(parts[0] == "a"_s);
    fm_assert(parts[1] == "b"_s);
    fm_assert(parts[2] == "c"_s);

    // The returned prefix is what makes this usable as a StringIterable.
    fm_assert("/"_s.join(parts) == "a/b/c"_s);

    StringView buf2[3];
    const auto parts2 = split_string_into(buf2, split_string("a,b,c"_s, ','));
    fm_assert(parts2.data() == buf2);
    fm_assert(parts2.size() == 3);

    StringView buf3[4];
    fm_assert(split_string_into(buf3, split_string(",,"_s, ',')).size() == 3);

    StringView buf4[4];
    fm_assert(split_string_into(buf4, split_string(""_s, ',')).isEmpty());
}

constexpr StringView substring_inputs[] = {
    ""_s, "::"_s, "a::b"_s, "a::::b"_s, "::a"_s, "a::"_s, "::a::::b::"_s,
    "aaa"_s, "aaaa"_s, "abab"_s, "abcabc"_s, "a"_s, "a, b, c"_s,
    "x\r\ny\nz"_s, "\r\n"_s, "a\r\n\r\nb"_s,
};

constexpr StringView substring_delimiters[] = {
    "::"_s, ", "_s, "ab"_s, "aa"_s, "abc"_s, "a"_s, "\r\n"_s,
};

// Corrade's split(StringView) is keep-empty and asserts on an empty delimiter, so it only
// covers that half. The without-empty-parts and empty-delimiter cases are checked by hand.
void test_substring_against_corrade()
{
    for (auto str : substring_inputs)
        for (auto delim : substring_delimiters)
            compare(split_string_on_substring(str, delim), str.split(delim));

    for (auto str : inputs)
        for (auto delim : substring_delimiters)
            compare(split_string_on_substring(str, delim), str.split(delim));
}

void test_substring_empty_delimiter()
{
    // stringFindString() reports an empty needle at every position. Nothing is a delimiter
    // instead, matching the empty character set.
    for (auto str : inputs)
    {
        const StringView whole[] = { str };
        const auto expected = str.isEmpty() ? ArrayView<const StringView>{} : whole;
        compare(split_string_on_substring(str, ""_s), expected);
        compare(split_string_on_substring(str, StringView{}), expected);
        compare(split_string_on_substring_without_empty_parts(str, ""_s), expected);
    }
}

void test_substring_without_empty_parts()
{
    constexpr auto str = "::a::::b::"_s;
    constexpr part keep[] = { {0, 0}, {2, 1}, {5, 0}, {7, 1}, {10, 0} };
    compare(split_string_on_substring(str, "::"_s), str, keep);

    constexpr part drop[] = { {2, 1}, {7, 1} };
    compare(split_string_on_substring_without_empty_parts(str, "::"_s), str, drop);
}

void test_substring_overlapping()
{
    // Matches are consumed whole, left to right, so "aa" in "aaa" leaves one 'a' behind.
    constexpr auto str = "aaa"_s;
    constexpr part expected[] = { {0, 0}, {2, 1} };
    compare(split_string_on_substring(str, "aa"_s), str, expected);
}

void test_substring_longer_than_input()
{
    constexpr auto str = "ab"_s;
    const StringView whole[] = { str };
    compare(split_string_on_substring(str, "abcd"_s), whole);
}

void test_any_substring()
{
    // Both line endings in one list. The longer match wins the tie whatever order it is in.
    constexpr auto str = "a\r\nb\nc\r\n"_s;
    constexpr StringView eol_a[] = { "\n"_s, "\r\n"_s };
    constexpr StringView eol_b[] = { "\r\n"_s, "\n"_s };
    constexpr part keep[] = { {0, 1}, {3, 1}, {5, 1}, {8, 0} };
    compare(split_string_on_any_substring(str, eol_a), str, keep);
    compare(split_string_on_any_substring(str, eol_b), str, keep);

    constexpr part drop[] = { {0, 1}, {3, 1}, {5, 1} };
    compare(split_string_on_any_substring_without_empty_parts(str, eol_a), str, drop);
    compare(split_string_on_any_substring_without_empty_parts(str, eol_b), str, drop);
}

void test_any_substring_longest_wins()
{
    constexpr auto str = "xabcy"_s;
    constexpr StringView shorter_first[] = { "ab"_s, "abc"_s };
    constexpr StringView longer_first[] = { "abc"_s, "ab"_s };
    constexpr part expected[] = { {0, 1}, {4, 1} };
    compare(split_string_on_any_substring(str, shorter_first), str, expected);
    compare(split_string_on_any_substring(str, longer_first), str, expected);
}

void test_any_substring_leftmost_wins()
{
    // Position beats length: "yy" starts earlier than the longer "zzz".
    constexpr auto str = "ayybzzzc"_s;
    constexpr StringView delims[] = { "zzz"_s, "yy"_s };
    constexpr part expected[] = { {0, 1}, {3, 1}, {7, 1} };
    compare(split_string_on_any_substring(str, delims), str, expected);
}

void test_any_substring_degenerate()
{
    constexpr StringView one_empty[] = { ""_s };
    constexpr StringView two_empty[] = { ""_s, StringView{} };

    for (auto str : inputs)
    {
        const StringView whole[] = { str };
        const auto expected = str.isEmpty() ? ArrayView<const StringView>{} : whole;
        // Spelled out because a bare {} is an identity conversion to both overloads.
        compare(split_string_on_any_substring(str, ArrayView<const StringView>{}), expected);
        compare(split_string_on_any_substring(str, one_empty), expected);
        compare(split_string_on_any_substring(str, two_empty), expected);
        compare(split_string_on_any_substring_without_empty_parts(str, two_empty), expected);
    }

    // An empty entry alongside a real one is skipped, not treated as a match everywhere.
    constexpr auto str = "a,b"_s;
    constexpr StringView mixed[] = { ""_s, ","_s };
    constexpr part expected[] = { {0, 1}, {2, 1} };
    compare(split_string_on_any_substring(str, mixed), str, expected);
}

void test_any_substring_narrowing()
{
    // A one-entry list narrows to the substring form, and a one-byte needle to the char form.
    // All four spellings have to agree.
    constexpr StringView one_comma[] = { ","_s };
    for (auto str : inputs)
    {
        compare(split_string_on_any_substring(str, one_comma), str.split(','));
        compare(split_string_on_substring(str, ","_s), str.split(','));
        compare(split_string_on_any_substring_without_empty_parts(str, one_comma), str.splitWithoutEmptyParts(','));
        compare(split_string_on_substring_without_empty_parts(str, ","_s), str.splitWithoutEmptyParts(','));
    }

    constexpr StringView one_colon[] = { "::"_s };
    for (auto str : substring_inputs)
        compare(split_string_on_any_substring(str, one_colon), str.split("::"_s));
}

void test_substring_alignment_sweep()
{
    // stringFindString() memcmp's at every position; walk a 3-byte needle across the buffer so
    // an overread past buf shows up as an ASan redzone hit.
    char buf[71];
    for (auto& c : buf)
        c = 'a';
    for (uint32_t len = 3; len <= array_size(buf); len++)
        for (uint32_t at = 0; at + 3 <= len; at++)
        {
            buf[at] = 'b'; buf[at+1] = 'c'; buf[at+2] = 'd';
            const StringView str{buf, len};
            const part expected[] = { {0, at}, {at + 3, len - at - 3} };
            compare(split_string_on_substring(str, "bcd"_s), str, expected);
            const StringView delims[] = { "bcd"_s, "cd"_s };
            compare(split_string_on_any_substring(str, delims), str, expected);
            buf[at] = 'a'; buf[at+1] = 'a'; buf[at+2] = 'a';
        }
}

void test_substring_containers()
{
    constexpr auto str = "a::bb::::ccc"_s;
    const split_array arr{split_string_on_substring_without_empty_parts(str, "::"_s)};
    fm_assert(arr.size() == 3);
    fm_assert("/"_s.join(arr) == "a/bb/ccc"_s);

    constexpr StringView delims[] = { "::"_s, "--"_s };
    StringView buf[8];
    const auto parts = split_string_into(buf, split_string_on_any_substring_without_empty_parts("a--b::c"_s, delims));
    fm_assert(parts.size() == 3);
    fm_assert("/"_s.join(parts) == "a/b/c"_s);
}

void test_any_substring_initializer_list()
{
    constexpr auto str = "a\r\nb\nc\r\n"_s;
    constexpr part keep[] = { {0, 1}, {3, 1}, {5, 1}, {8, 0} };
    compare(split_string_on_any_substring(str, {"\n"_s, "\r\n"_s}), str, keep);
    compare(split_string_on_any_substring(str, {"\r\n"_s, "\n"_s}), str, keep);

    constexpr part drop[] = { {0, 1}, {3, 1}, {5, 1} };
    compare(split_string_on_any_substring_without_empty_parts(str, {"\n"_s, "\r\n"_s}), str, drop);

    // A one-entry braced list narrows to the char form, same as a one-entry array.
    for (auto s : inputs)
    {
        compare(split_string_on_any_substring(s, {","_s}), s.split(','));
        compare(split_string_on_any_substring_without_empty_parts(s, {","_s}), s.splitWithoutEmptyParts(','));
    }

    const split_array arr{split_string_on_any_substring_without_empty_parts("a::bb--ccc"_s, {"::"_s, "--"_s})};
    fm_assert(arr.size() == 3);
    fm_assert("/"_s.join(arr) == "a/bb/ccc"_s);

    StringView buf[8];
    const auto tokens = split_string_into(buf, split_string_on_any_substring("x||y|z"_s, {"||"_s, "|"_s}));
    fm_assert(tokens.size() == 3);
    fm_assert("/"_s.join(tokens) == "x/y/z"_s);
}

} // namespace

void Test::test_split()
{
    test_against_corrade();
    test_empty_delimiter_set();
    test_keep_empty_on_sets();
    test_iterator();
    test_multipass();
    test_ranges();
    test_split_array();
    test_split_array_full();
    test_embedded_nul();
    test_non_ascii();
    test_alignment_sweep();
    test_long_input();
    test_split_into();
    test_substring_against_corrade();
    test_substring_empty_delimiter();
    test_substring_without_empty_parts();
    test_substring_overlapping();
    test_substring_longer_than_input();
    test_any_substring();
    test_any_substring_longest_wins();
    test_any_substring_leftmost_wins();
    test_any_substring_degenerate();
    test_any_substring_narrowing();
    test_substring_alignment_sweep();
    test_substring_containers();
    test_any_substring_initializer_list();
}

} // namespace floormat
