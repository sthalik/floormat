#include "app.hpp"
#include "compat/array-size.hpp"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include "compat/range-access.hpp"
#include "compat/sorted-string-map.inl"
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <cr/Pointer.h>
#include <cr/String.h>
#include <cr/StructuredBindings.h>

namespace floormat::Test {

struct NonCopyable
{
    int val;
    NonCopyable(int val): val{val} {}
    fm_DISABLE_COPY(NonCopyable);
    fm_DEFAULT_MOVE(NonCopyable);
};

struct three_ints
{
    int a, b, c;

    bool operator==(const three_ints&) const = default;
};

constexpr u32 alive_magic = 0x600dc0de, dead_magic = 0xdeadbeef;

struct counted
{
    static inline int live = 0;
    u32 magic;
    int value;

    counted(int value): magic{alive_magic}, value{value} { live++; }

    counted(const counted& other): magic{alive_magic}, value{other.value}
    {
        fm_assert(other.magic == alive_magic);
        live++;
    }

    counted(counted&& other) noexcept: magic{alive_magic}, value{other.value}
    {
        fm_assert(other.magic == alive_magic);
        other.value = -2;
        live++;
    }

    counted& operator=(const counted& other)
    {
        fm_assert(magic == alive_magic && other.magic == alive_magic);
        value = other.value;
        return *this;
    }

    counted& operator=(counted&& other) noexcept
    {
        fm_assert(magic == alive_magic && other.magic == alive_magic);
        value = other.value;
        other.value = -2;
        return *this;
    }

    ~counted() noexcept
    {
        fm_assert(magic == alive_magic);
        magic = dead_magic;
        live--;
    }
};

} // namespace floormat::Test

namespace floormat {

template class sorted_string_map<int>;
template class sorted_string_map<Test::three_ints>;
template class sorted_string_map<Test::NonCopyable>;
template class sorted_string_map<Test::counted>;
template class sorted_string_map<String>;
template class sorted_string_map<Pointer<int>>;

} // namespace floormat

namespace floormat::Test {

namespace {

using int_map = sorted_string_map<int>;

static_assert(std::random_access_iterator<int_map::iterator>);
static_assert(std::random_access_iterator<int_map::const_iterator>);
static_assert(std::random_access_iterator<int_map::reverse_iterator>);
static_assert(std::random_access_iterator<int_map::const_reverse_iterator>);
static_assert(random_access_iterator<int_map::iterator>);
static_assert(random_access_range<int_map&>);
static_assert(random_access_range<const int_map&>);
static_assert(std::is_convertible_v<int_map::iterator, int_map::const_iterator>);
static_assert(!std::is_convertible_v<int_map::const_iterator, int_map::iterator>);
static_assert(std::is_same_v<decltype(std::declval<int_map&>().begin()->second), int&>);
static_assert(std::is_same_v<decltype(std::declval<const int_map&>().begin()->second), const int&>);
static_assert(std::is_same_v<decltype(std::declval<int_map&>().rbegin()->second), int&>);
static_assert(std::is_trivially_copyable_v<NonCopyable>);
static_assert(!std::is_copy_constructible_v<sorted_string_map<NonCopyable>>);
static_assert(!std::is_copy_constructible_v<sorted_string_map<Pointer<int>>>);
static_assert(std::is_nothrow_move_constructible_v<sorted_string_map<Pointer<int>>>);
static_assert(std::is_copy_constructible_v<sorted_string_map<String>>);
static_assert(std::is_trivially_copyable_v<int_map::iterator> && std::is_trivially_copyable_v<int_map::const_iterator>);

template<typename M, typename V> concept can_insert = requires(M& m, V&& value) { m.insert(forward<V>(value)); };

static_assert(can_insert<int_map, std::pair<StringView, int>&>);
static_assert(can_insert<int_map, const std::pair<StringView, int>&>);
static_assert(can_insert<int_map, Pair<StringView, int>&>);
static_assert(can_insert<int_map, int_map::reference>);
static_assert(!can_insert<int_map, int>);
static_assert(!can_insert<int_map, StringView>);

StringView view(const std::string& s) { return StringView{s.data(), s.size()}; }
std::string number_key(int i) { return "key " + std::to_string(i); }

String long_value(int i)
{
    const auto s = "value " + std::to_string(i) + " with enough characters to need a heap allocation";
    return String{s.data(), s.size()};
}

template<typename T>
bool has_keys_in_order(const sorted_string_map<T>& m, std::initializer_list<StringView> keys)
{
    bool ret = m.size() == keys.size();
    const StringView* key = keys.begin();
    for (auto it = m.begin(); ret && it != m.end(); ++it, ++key)
        ret = it->first == *key;
    return ret;
}

void check_same(const int_map& m, const std::map<std::string, int>& expected)
{
    fm_assert(m.size() == expected.size());
    fm_assert(m.empty() == expected.empty());
    auto it = m.begin();
    for (const auto& [key, value] : expected)
    {
        fm_assert(it != m.end());
        fm_assert(it->first == view(key));
        fm_assert(it->first.flags() & StringViewFlag::NullTerminated);
        fm_assert(it->first.data()[it->first.size()] == '\0');
        fm_assert(it->second == value);
        fm_assert(m.find(view(key)) == it);
        ++it;
    }
    fm_assert(it == m.end());
}

void test_against_std_map()
{
    std::vector<std::string> keys = {
        "", "a", "aa", "aaa", "ab", "abc", "b", "ba", "bab",
        std::string("a\0b", 3), std::string("a\0", 2), std::string(100, 'z'),
    };
    for (int i = 0; i < 150; i++)
        keys.push_back(number_key(i * 7919 % 1009) + std::string((usize)(i % 13), '#'));

    int_map m;
    std::map<std::string, int> expected;
    u32 state = 1;
    auto next = [&](u32 n) {
        state = state * 1664525u + 1013904223u;
        return (state >> 8) % n;
    };

    for (int step = 0; step < 3000; step++)
    {
        const auto& key = keys[next((u32)keys.size())];
        switch (next(7))
        {
        case 0: {
            auto [it, inserted] = m.try_emplace(view(key), step);
            fm_assert(inserted == expected.try_emplace(key, step).second);
            fm_assert(it->first == view(key));
            break;
        }
        case 1: {
            auto [it, inserted] = m.insert_or_assign(view(key), step);
            fm_assert(inserted == expected.insert_or_assign(key, step).second);
            fm_assert(it->second == step);
            break;
        }
        case 2:
            m[view(key)] += 3;
            expected[key] += 3;
            break;
        case 3:
            fm_assert(m.erase(view(key)) == expected.erase(key));
            break;
        case 4:
            if (auto it = m.find(view(key)); it != m.end())
            {
                auto next_it = m.erase(it);
                auto expected_next = expected.erase(expected.find(key));
                fm_assert(next_it == m.end() ? expected_next == expected.end()
                                             : next_it->first == view(expected_next->first));
            }
            break;
        case 5:
            fm_assert(m.insert(m.lower_bound(view(key)), {view(key), step})->first == view(key));
            expected.try_emplace(key, step);
            break;
        case 6:
            fm_assert(m.emplace_hint(m.begin() + next(m.size() + 1), view(key), step)->first == view(key));
            expected.try_emplace(key, step);
            break;
        }
        check_same(m, expected);
    }

    while (!m.empty())
    {
        const auto& key = keys[next((u32)keys.size())];
        fm_assert(m.erase(view(key)) == expected.erase(key));
        check_same(m, expected);
    }
}

void test_iterators()
{
    int_map m = {{"d", 4}, {"b", 2}, {"f", 6}};
    const auto& cm = m;

    fm_assert(m.end() - m.begin() == 3);
    fm_assert(m.begin()[1].first == "d"_s);
    fm_assert((1 + m.begin())->second == 4);
    auto it = m.end();
    --it;
    fm_assert(it->first == "f"_s);
    fm_assert(it-- == m.begin() + 2);
    fm_assert(it->first == "d"_s);
    fm_assert(m.begin() < it && it <= m.begin() + 1 && it > cm.begin() && cm.end() >= it);
    const int_map::const_iterator cit = m.begin();
    fm_assert(cit == m.begin() && m.begin() == cit && cit != m.end());

    const StringView reversed[] = {"f"_s, "d"_s, "b"_s};
    u32 i = 0;
    for (auto r = cm.rbegin(); r != cm.rend(); ++r)
        fm_assert(r->first == reversed[i++]);
    fm_assert(i == 3);
    fm_assert(m.rbegin()->second == 6 && (*m.crbegin()).first == "f"_s && m.rend() - m.rbegin() == 3);

    for (auto [key, value] : m)
        value *= 10;
    fm_assert(m.at("b") == 20 && m.at("d") == 40 && m.at("f") == 60);
    const auto& [key, value] = *cm.begin();
    static_assert(std::is_same_v<decltype(value), const int&>);
    fm_assert(key == "b"_s && &value == &m.at("b"));
}

void test_lookup()
{
    const int_map m = {{"b", 2}, {"d", 4}, {"f", 6}};

    fm_assert(m.lower_bound("a") == m.begin());
    fm_assert(m.lower_bound("b") == m.begin());
    fm_assert(m.lower_bound("c")->first == "d"_s);
    fm_assert(m.lower_bound("g") == m.end());
    fm_assert(m.upper_bound("b")->first == "d"_s);
    fm_assert(m.upper_bound("c")->first == "d"_s);
    fm_assert(m.upper_bound("f") == m.end());
    auto [first, last] = m.equal_range("d");
    fm_assert(first->first == "d"_s && last->first == "f"_s);
    auto [first2, last2] = m.equal_range("e");
    fm_assert(first2 == last2 && first2->first == "f"_s);
    fm_assert(m.find("c") == m.end() && m.find("d")->second == 4);
    fm_assert(m.count("d") == 1 && m.count("e") == 0);
    fm_assert(m.contains("f") && !m.contains(""));
    fm_assert(m.at("f") == 6);
    fm_assert(m.size() == 3 && !m.empty() && m.max_size() == (u32)-1);
    const int_map empty_map;
    fm_assert(empty_map.empty() && empty_map.begin() == empty_map.end());
}

void test_insert()
{
    int_map m = {{"b", 1}, {"a", 2}, {"b", 3}};
    fm_assert(has_keys_in_order(m, {"a"_s, "b"_s}) && m.at("b") == 1);

    auto [it, inserted] = m.insert({"c", 4});
    fm_assert(inserted && it->first == "c"_s && it->second == 4);
    auto [it2, inserted2] = m.insert({"c", 5});
    fm_assert(!inserted2 && it2 == it && it->second == 4);
    const Pair<StringView, int> d{"d", 6};
    fm_assert(m.insert(d).second());
    fm_assert(m.insert(m.end(), {"e", 7})->first == "e"_s);
    fm_assert(m.insert(m.begin(), {"f", 8})->first == "f"_s);
    fm_assert(m.insert(m.cend(), d)->second == 6);
    fm_assert(m.emplace("g", 9).second() && !m.emplace("g", 10).second() && m.at("g") == 9);
    fm_assert(m.emplace_hint(m.begin(), "0", 11)->first == "0"_s);
    auto [it3, inserted3] = m.insert_or_assign("g", 12);
    fm_assert(!inserted3 && it3->second == 12);
    fm_assert(m.insert_or_assign(m.end(), "h", 13)->second == 13);
    fm_assert(m.insert_or_assign(m.begin(), "h", 14)->second == 14);
    fm_assert(m.try_emplace(m.end(), "h", 15)->second == 14);
    m["i"];
    fm_assert(m.at("i") == 0);
    m["i"] = 16;
    fm_assert(m.at("i") == 16);

    const std::vector<Pair<StringView, int>> pairs = {{"j", 17}, {"a", 18}};
    m.insert(pairs.begin(), pairs.end());
    fm_assert(m.at("j") == 17 && m.at("a") == 2);
    const int_map other = {{"k", 19}, {"a", 20}};
    m.insert(other.begin(), other.end());
    fm_assert(m.at("k") == 19 && m.at("a") == 2);
    const std::pair<StringView, int> std_pairs[] = {{"l", 21}, {"a", 22}};
    m.insert_range(std_pairs);
    m.insert_range(int_map{{"m", 23}});
    fm_assert(m.at("l") == 21 && m.at("m") == 23 && m.at("a") == 2);
    m.insert({{"n", 24}, {"a", 25}});
    fm_assert(m.at("n") == 24 && m.at("a") == 2);

    std::pair<StringView, int> std_pair{"o", 26};
    auto [it4, inserted4] = m.insert(std_pair);
    fm_assert(inserted4 && it4->first == "o"_s && it4->second == 26);
    Pair<StringView, int> pair{"p", 27};
    fm_assert(m.insert(pair).second() && !m.insert(pair).second() && m.at("p") == 27);
    fm_assert(m.insert(m.end(), std::pair<StringView, int>{"q", 28})->second == 28);
    fm_assert(m.insert(m.begin(), std::pair<StringView, int>{"q", 29})->second == 28);
    fm_assert(!m.insert(*other.begin()).second() && m.at("a") == 2);
    fm_assert(m.insert(m.end(), *other.begin())->second == 2);
    fm_assert(has_keys_in_order(m, {"0"_s, "a"_s, "b"_s, "c"_s, "d"_s, "e"_s, "f"_s, "g"_s, "h"_s, "i"_s,
                                    "j"_s, "k"_s, "l"_s, "m"_s, "n"_s, "o"_s, "p"_s, "q"_s}));

    m = {{"x", 1}};
    fm_assert(has_keys_in_order(m, {"x"_s}) && m.at("x") == 1);
}

void test_erase()
{
    const char digits[] = "0123456789";
    int_map m;
    for (int i = 0; i < 10; i++)
        m.try_emplace(StringView{digits + i, 1}, i);
    const char* key_buffer = m.begin()->first.data();

    auto it = m.erase(m.begin());
    fm_assert(it == m.begin() && it->first == "1"_s);
    it = m.erase(m.find("5"));
    fm_assert(it->first == "6"_s);
    it = m.erase(m.find("9"));
    fm_assert(it == m.end());
    it = m.erase(m.find("2"), m.find("4"));
    fm_assert(it->first == "4"_s);
    fm_assert(m.erase(m.end(), m.end()) == m.end());
    fm_assert(m.erase("4") == 1 && m.erase("4") == 0);
    fm_assert(has_keys_in_order(m, {"1"_s, "6"_s, "7"_s, "8"_s}));
    fm_assert(m.at("6") == 6 && m.at("8") == 8);

    fm_assert(erase_if(m, [](const auto& x) { return x.second % 2 == 0; }) == 2);
    fm_assert(has_keys_in_order(m, {"1"_s, "7"_s}) && m.at("7") == 7);
    it = m.erase(m.begin(), m.end());
    fm_assert(it == m.end() && m.empty());
    m.try_emplace("z", 1);
    fm_assert(has_keys_in_order(m, {"z"_s}) && m.begin()->first.data() == key_buffer);
}

void test_copy_move_swap()
{
    sorted_string_map<String> a;
    for (int i = 0; i < 20; i++)
        a.try_emplace(long_value(i), long_value(i));

    sorted_string_map<String> b{a};
    fm_assert(a == b);
    b.at(long_value(3)) = String{"changed"_s};
    fm_assert(a != b);
    fm_assert(a.at(long_value(3)) == long_value(3));

    sorted_string_map<String> c{move(b)};
    fm_assert(b.empty() && c.size() == 20 && c.at(long_value(3)) == "changed"_s);
    b.try_emplace("x", "y"_s);
    swap(b, c);
    fm_assert(b.size() == 20 && has_keys_in_order(c, {"x"_s}) && c.at("x") == "y"_s);

    c = b;
    fm_assert(c == b);
    c = move(a);
    fm_assert(c.size() == 20 && c.at(long_value(3)) == long_value(3));
    auto& alias = c;
    c = alias;
    c = move(alias);
    fm_assert(c.size() == 20 && c.at(long_value(3)) == long_value(3));

    const char* key_buffer = c.begin()->first.data();
    c.clear();
    fm_assert(c.empty() && c.begin() == c.end() && !c.contains(long_value(3)));
    c.try_emplace("after clear", "z"_s);
    fm_assert(has_keys_in_order(c, {"after clear"_s}) && c.begin()->first.data() == key_buffer);
}

void test_aliasing()
{
    sorted_string_map<String> m;
    for (int i = 0; i < 8; i++)
        m.try_emplace(long_value(i), long_value(i));

    // The 9th entry exceeds the first array capacity, and "a key" sorts first.
    m.try_emplace("a key", m.at(long_value(5)));
    fm_assert(m.at("a key") == long_value(5) && m.at(long_value(5)) == long_value(5));
    m.try_emplace("b key", m.at(long_value(6)));
    fm_assert(m.at("b key") == long_value(6) && m.at(long_value(6)) == long_value(6));
    m.insert_or_assign("c key", m.at(long_value(7)));
    m.insert_or_assign(long_value(0), m.at(long_value(1)));
    fm_assert(m.at("c key") == long_value(7) && m.at(long_value(0)) == long_value(1));

    m.try_emplace(m.begin()->first.prefix(1), long_value(100));
    fm_assert(m.begin()->first == "a"_s && m.at("a key") == long_value(5));
    const StringView straddling{m.begin()->first.data(), 4};
    const String copy = straddling;
    fm_assert(copy == "a\0a "_s);
    m.try_emplace(straddling, long_value(101));
    fm_assert(m.at(copy) == long_value(101) && m.at("a") == long_value(100) && m.at("a key") == long_value(5));
    fm_assert(&m[m.begin()->first] == &m.begin()->second);
    m[m.at("b key").prefix(5)] = long_value(102);
    fm_assert(m.at("value") == long_value(102));
    fm_assert(has_keys_in_order(m, {"a"_s, copy, "a key"_s, "b key"_s, "c key"_s, "value"_s, long_value(0),
                                    long_value(1), long_value(2), long_value(3), long_value(4), long_value(5),
                                    long_value(6), long_value(7)}));
}

void test_keys_viewing_key_buffer()
{
    std::string ascending;
    for (char c = '!'; c <= 'z'; c++)
        ascending += c;
    const auto n = (int)ascending.size();

    int_map at_end, at_start, in_middle;
    std::map<std::string, int> expected_at_end, expected_at_start, expected_in_middle;
    at_end.try_emplace(view(ascending), 0);
    expected_at_end.try_emplace(ascending, 0);
    at_start.try_emplace(view(ascending), 0);
    expected_at_start.try_emplace(ascending, 0);
    in_middle.try_emplace("", 0);
    in_middle.try_emplace(view(ascending), 0);
    expected_in_middle.try_emplace("", 0);
    expected_in_middle.try_emplace(ascending, 0);

    for (int i = 1; i < n; i++)
    {
        auto [it, inserted] = at_end.try_emplace(at_end.rbegin()->first.exceptPrefix(1), i);
        fm_assert(inserted && it + 1 == at_end.end());
        expected_at_end.try_emplace(ascending.substr((usize)i), i);
        check_same(at_end, expected_at_end);

        auto [it2, inserted2] = at_start.try_emplace(at_start.begin()->first.exceptSuffix(1), i);
        fm_assert(inserted2 && it2 == at_start.begin());
        expected_at_start.try_emplace(ascending.substr(0, (usize)(n - i)), i);
        check_same(at_start, expected_at_start);

        auto [it3, inserted3] = in_middle.try_emplace(in_middle.rbegin()->first.prefix((usize)(n - i)), i);
        fm_assert(inserted3 && it3 == in_middle.begin() + 1);
        expected_in_middle.try_emplace(ascending.substr(0, (usize)(n - i)), i);
        check_same(in_middle, expected_in_middle);
    }

    int_map spanning_offset;
    std::map<std::string, int> expected_spanning_offset;
    for (char c = 'a'; c <= 'y'; c += 2)
    {
        spanning_offset.try_emplace(StringView{&c, 1}, 0);
        expected_spanning_offset.try_emplace(std::string(1, c), 0);
    }
    for (char c = 'w'; c >= 'a'; c -= 2)
    {
        const StringView key{spanning_offset.find(StringView{&c, 1})->first.data(), 3};
        const std::string expected_key{key.data(), key.size()};
        auto [it, inserted] = spanning_offset.try_emplace(key, c);
        fm_assert(inserted && it == spanning_offset.find(StringView{&c, 1}) + 1 && it->first == view(expected_key));
        expected_spanning_offset.try_emplace(expected_key, c);
        check_same(spanning_offset, expected_spanning_offset);
    }

    int_map before_offset{{"az", 0}, {"b", 1}, {"zz", 2}};
    auto [it, inserted] = before_offset.try_emplace(before_offset.begin()->first.exceptPrefix(1), 3);
    fm_assert(inserted && it == before_offset.begin() + 2);
    fm_assert(has_keys_in_order(before_offset, {"az"_s, "b"_s, "z"_s, "zz"_s}));
}

int value_of(int value) { return value; }

int value_of(const counted& value)
{
    fm_assert(value.magic == alive_magic);
    return value.value;
}

template<typename T, usize N>
void check_keys(const sorted_string_map<T>& m, const std::string (&keys)[N], u32 present_keys)
{
    u32 i = 0;
    for (auto [k, v] : m)
    {
        while (i < N && !(present_keys >> i & 1))
            i++;
        fm_assert(i < N && k == view(keys[i]));
        fm_assert(k.flags() & StringViewFlag::NullTerminated && k.data()[k.size()] == '\0');
        fm_assert(value_of(v) == (int)i);
        i++;
    }
    fm_assert(present_keys >> i == 0);
}

template<typename T>
void test_insert_paths()
{
    // Must equal min_string_buffer_capacity, so a padded map's key buffer has free_bytes bytes left.
    constexpr u32 buffer_capacity = 64;
    const std::string small_keys[] = {"", "a", "ab", "b", "ba"};
    const std::string literal_keys[] = {"", "a", "aa", "ab", "b", "ba", "c", "d", std::string(buffer_capacity, 'e')};
    enum class setup : u8 { fresh, reserved_for_keys, reserved_with_spare, cleared, };

    for (u32 subset = 0; subset < 1u << array_size(small_keys); subset++)
        for (int free_bytes = -1; free_bytes <= 2; free_bytes++)
            for (setup s : {setup::fresh, setup::reserved_for_keys, setup::reserved_with_spare, setup::cleared})
            {
                std::vector<std::string> keys;
                u32 buffer_length = 0;
                for (u32 i = 0; i < array_size(small_keys); i++)
                    if (subset & 1u << i)
                    {
                        keys.push_back(small_keys[i]);
                        buffer_length += (u32)small_keys[i].size() + 1;
                    }
                const u32 small_keys_length = buffer_length;
                if (free_bytes >= 0)
                {
                    keys.push_back(std::string(buffer_capacity - (u32)free_bytes - buffer_length - 1, 'c'));
                    buffer_length = buffer_capacity - (u32)free_bytes;
                }

                if (s == setup::cleared && free_bytes >= 0)
                    continue;
                auto fill_map = [&](sorted_string_map<T>& m) {
                    if (s == setup::reserved_for_keys)
                        m.reserve((u32)keys.size());
                    else if (s == setup::reserved_with_spare)
                        m.reserve((u32)keys.size() + 1);
                    else if (s == setup::cleared)
                    {
                        m.try_emplace("x", -1);
                        m.clear();
                    }
                    for (u32 i = 0; i < keys.size(); i++)
                        m.try_emplace(view(keys[i]), (int)i);
                };
                auto test_key = [&](auto get_key) {
                    sorted_string_map<T> m;
                    fill_map(m);
                    const StringView key = get_key(m);
                    const std::string key_copy{key.data(), key.size()};
                    u32 pos = 0;
                    while (pos < keys.size() && keys[pos] < key_copy)
                        pos++;
                    const bool is_new = pos == keys.size() || keys[pos] != key_copy;
                    auto [it, inserted] = m.try_emplace(key, 100);
                    fm_assert(inserted == is_new && it == m.begin() + (isize)pos && it->first == view(key_copy));
                    fm_assert(m.size() == keys.size() + (u32)is_new);
                    u32 i = 0;
                    for (auto [k, v] : m)
                    {
                        const bool is_inserted_key = is_new && i == pos;
                        const u32 j = is_new && i > pos ? i - 1 : i;
                        fm_assert(k == (is_inserted_key ? view(key_copy) : view(keys[j])));
                        fm_assert(k.flags() & StringViewFlag::NullTerminated && k.data()[k.size()] == '\0');
                        fm_assert(value_of(v) == (is_inserted_key ? 100 : (int)j));
                        i++;
                    }
                };

                for (const auto& key : literal_keys)
                    test_key([&](const sorted_string_map<T>&) { return view(key); });
                for (u32 size = 0; size <= 3; size++)
                    for (u32 start = 0; start < buffer_length && start + size <= buffer_length; start++)
                        if (size > 0 && (start < small_keys_length + 3 || start + 4 >= buffer_length) || start == 0)
                            test_key([&](const sorted_string_map<T>& m) {
                                return StringView{m.begin()->first.data() + start, size};
                            });
            }
    if constexpr (std::is_same_v<T, counted>)
        fm_assert(counted::live == 0);
}

template<typename T>
void test_erase_if_paths()
{
    const std::string keys[] = {"", "a", "ab", "b", "ba", std::string(70, 'c'), "d"};
    constexpr u32 num_keys = (u32)array_size(keys), all_keys = (1u << num_keys) - 1;

    sorted_string_map<T> empty;
    fm_assert(erase_if(empty, [](const auto&) { return true; }) == 0 && empty.empty());

    for (u32 removed_keys = 0; removed_keys <= all_keys; removed_keys++)
    {
        sorted_string_map<T> m;
        for (u32 i = 0; i < num_keys; i++)
            m.try_emplace(view(keys[i]), (int)i);
        const char* key_buffer = m.begin()->first.data();

        u32 num_pred_calls = 0, num_removed = 0;
        const u32 ret = erase_if(m, [&](const auto& x) {
            const u32 i = num_pred_calls++;
            fm_assert(x.first == view(keys[i]) && value_of(x.second) == (int)i);
            num_removed += removed_keys >> i & 1;
            return (removed_keys >> i & 1) != 0;
        });
        fm_assert(ret == num_removed && num_pred_calls == num_keys && m.size() == num_keys - num_removed);

        check_keys(m, keys, all_keys & ~removed_keys);
        for (u32 i = 0; i < num_keys; i++)
            if (removed_keys >> i & 1)
                m.try_emplace(view(keys[i]), (int)i);
        check_keys(m, keys, all_keys);
        fm_assert(m.begin()->first.data() == key_buffer);
    }
    if constexpr (std::is_same_v<T, counted>)
        fm_assert(counted::live == 0);
}

template<typename T>
void test_erase_range_paths()
{
    const std::string keys[] = {"", "a", "ab", "b", "ba", std::string(70, 'c'), "d"};
    constexpr u32 num_keys = (u32)array_size(keys), all_keys = (1u << num_keys) - 1;

    for (u32 first = 0; first <= num_keys; first++)
        for (u32 last = first; last <= num_keys; last++)
        {
            sorted_string_map<T> m;
            for (u32 i = 0; i < num_keys; i++)
                m.try_emplace(view(keys[i]), (int)i);
            const char* key_buffer = m.begin()->first.data();

            const auto it = m.erase(m.cbegin() + (isize)first, m.cbegin() + (isize)last);
            fm_assert(it == m.begin() + (isize)first && m.size() == num_keys - (last - first));
            const u32 removed_keys = (1u << last) - (1u << first);
            check_keys(m, keys, all_keys & ~removed_keys);
            for (u32 i = first; i < last; i++)
                m.try_emplace(view(keys[i]), (int)i);
            check_keys(m, keys, all_keys);
            fm_assert(m.begin()->first.data() == key_buffer);
        }
    if constexpr (std::is_same_v<T, counted>)
        fm_assert(counted::live == 0);
}

void test_value_types()
{
    sorted_string_map<NonCopyable> non_copyable;
    sorted_string_map<Pointer<int>> pointers;
    for (int i = 0; i < 30; i++)
    {
        const int n = i * 7 % 30;
        non_copyable.try_emplace(view(number_key(n)), n);
        pointers.try_emplace(view(number_key(n)), InPlaceInit, n);
    }
    for (int i = 0; i < 30; i += 2)
    {
        fm_assert(non_copyable.erase(view(number_key(i))) == 1);
        fm_assert(pointers.erase(view(number_key(i))) == 1);
    }
    auto moved_non_copyable = move(non_copyable);
    auto moved_pointers = move(pointers);
    fm_assert(non_copyable.empty() && pointers.empty());
    fm_assert(moved_non_copyable.size() == 15 && moved_pointers.size() == 15);
    for (int i = 1; i < 30; i += 2)
    {
        fm_assert(moved_non_copyable.at(view(number_key(i))).val == i);
        fm_assert(*moved_pointers.at(view(number_key(i))) == i);
    }
    std::pair<StringView, Pointer<int>> pointer_pair{"y", Pointer<int>{InPlaceInit, 31}};
    fm_assert(moved_pointers.insert(move(pointer_pair)).second() && !pointer_pair.second);
    fm_assert(*moved_pointers.at("y") == 31 && moved_pointers.size() == 16);

    sorted_string_map<three_ints> m;
    m.try_emplace("b", 1, 2, 3);
    m.emplace("a", 4, 5, 6);
    m.try_emplace("c");
    fm_assert(m.at("b") == three_ints{1, 2, 3} && m.at("a") == three_ints{4, 5, 6} && m.at("c") == three_ints{});
    sorted_string_map<three_ints> copy{m};
    fm_assert(copy == m);
    copy.at("c").b = 1;
    fm_assert(copy != m);
}

void test_live_count()
{
    {
        sorted_string_map<counted> m;
        for (int i = 0; i < 40; i++)
        {
            m.try_emplace(view(number_key(i * 17 % 40)), i * 17 % 40);
            fm_assert(counted::live == (int)m.size());
        }
        for (int i = 0; i < 40; i++)
            fm_assert(m.at(view(number_key(i))).value == i);
        for (int i = 0; i < 40; i += 3)
        {
            fm_assert(m.erase(view(number_key(i))) == 1);
            fm_assert(counted::live == (int)m.size());
        }
        {
            sorted_string_map<counted> copy{m};
            fm_assert(counted::live == 2 * (int)m.size());
            copy.insert_or_assign(view(number_key(1)), counted{-1});
            fm_assert(copy.at(view(number_key(1))).value == -1 && m.at(view(number_key(1))).value == 1);
        }
        fm_assert(counted::live == (int)m.size());

        sorted_string_map<counted> moved{move(m)};
        fm_assert(counted::live == (int)moved.size());
        erase_if(moved, [](const auto& x) { return x.second.value % 2 == 0; });
        fm_assert(counted::live == (int)moved.size());
        for (auto [key, value] : moved)
            fm_assert(value.magic == alive_magic && value.value % 2 != 0 && view(number_key(value.value)) == key);
        moved.clear();
        fm_assert(counted::live == 0);
        moved.try_emplace("x", 1);
        fm_assert(counted::live == 1);
    }
    fm_assert(counted::live == 0);
}

} // namespace

void test_sorted_string_map()
{
    test_against_std_map();
    test_iterators();
    test_lookup();
    test_insert();
    test_erase();
    test_copy_move_swap();
    test_aliasing();
    test_keys_viewing_key_buffer();
    test_insert_paths<int>();
    test_insert_paths<counted>();
    test_erase_if_paths<int>();
    test_erase_if_paths<counted>();
    test_erase_range_paths<int>();
    test_erase_range_paths<counted>();
    test_value_types();
    test_live_count();
}

} // namespace floormat::Test
