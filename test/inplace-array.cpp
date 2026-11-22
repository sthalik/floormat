#include "app.hpp"
#include "compat/inplace-array.inl"
#include <cr/Pointer.h>
#include <cr/String.h>
#include <cr/StringView.h>

namespace floormat {

namespace {

struct with_default_member
{
    int v = 7;
    constexpr bool operator==(const with_default_member&) const = default;
    constexpr auto operator<=>(const with_default_member&) const = default;
};

struct triple
{
    int a, b, c;
    constexpr triple(int i) : a{i}, b{i * 3}, c{i * 5} {}
    constexpr bool operator==(const triple&) const = default;
    constexpr auto operator<=>(const triple&) const = default;
};

struct less_only
{
    int v;
    constexpr less_only(int v) : v{v} {}
    constexpr bool operator==(const less_only&) const = default;
    constexpr bool operator<(const less_only& o) const { return v < o.v; }
};

struct equality_comparable_only
{
    int v;
    constexpr bool operator==(const equality_comparable_only&) const = default;
};

template<bool DefaultConstructible>
struct user_provided_destructor
{
    int v;
    constexpr user_provided_destructor() requires DefaultConstructible : v{7} {}
    constexpr user_provided_destructor(int v) : v{v} {}
    user_provided_destructor(const user_provided_destructor&) = default;
    user_provided_destructor& operator=(const user_provided_destructor&) = default;
    constexpr ~user_provided_destructor() {}
    constexpr bool operator==(const user_provided_destructor&) const = default;
    constexpr auto operator<=>(const user_provided_destructor&) const = default;
};

struct non_constexpr_default_ctor
{
    int v;
    non_constexpr_default_ctor() : v{3} {}
    constexpr non_constexpr_default_ctor(int v) : v{v} {}
};

template<bool DefaultConstructible>
struct counted
{
    int* live = nullptr;
    int v = 0;

    constexpr counted() requires DefaultConstructible = default;
    constexpr counted(int* live, int v) : live{live}, v{v} { add(1); }
    constexpr counted(const counted& o) : live{o.live}, v{o.v} { add(1); }
    constexpr counted(counted&& o) noexcept : live{o.live}, v{o.v} { add(1); }
    constexpr counted& operator=(const counted& o) { add(-1); live = o.live; v = o.v; add(1); return *this; }
    constexpr counted& operator=(counted&& o) noexcept { add(-1); live = o.live; v = o.v; add(1); return *this; }
    constexpr ~counted() { add(-1); }
    constexpr bool operator==(const counted& o) const { return v == o.v; }
    constexpr auto operator<=>(const counted& o) const { return v <=> o.v; }

    constexpr void add(int k)
    {
        if (live)
            *live += k;
    }
};

constexpr int value_of(int x) { return x; }
constexpr int value_of(const with_default_member& x) { return x.v; }
constexpr int value_of(const triple& x) { return x.a; }
template<bool D> constexpr int value_of(const counted<D>& x) { return x.v; }
template<bool D> constexpr int value_of(const user_provided_destructor<D>& x) { return x.v; }

template<typename T>
struct maker
{
    constexpr T operator()(int v) const { return T{v}; }
    template<typename F> constexpr decltype(auto) call_with_ctor_args(int v, F&& f) const { return f(v); }
    constexpr bool has_live_count(int) const { return true; }
};

template<bool D>
struct maker<counted<D>>
{
    int live = 0;

    constexpr counted<D> operator()(int v) { return counted<D>{&live, v}; }
    template<typename F> constexpr decltype(auto) call_with_ctor_args(int v, F&& f) { return f(&live, v); }
    constexpr bool has_live_count(int n) const { return live == n; }
};

template<typename T, usize N>
constexpr bool has_values(const inplace_array<T, N>& a, std::initializer_list<int> expected)
{
    if (a.size() != expected.size())
        return false;
    else
    {
        u32 i = 0;
        for (int x : expected)
            if (value_of(a[i++]) != x)
                return false;
        return true;
    }
}

static_assert(sizeof(inplace_array<u8, 7>) == 8);
static_assert(sizeof(inplace_array<u8, 300>) == 302);
static_assert(sizeof(inplace_array<u16, 3>) == 8);
static_assert(sizeof(inplace_array<u32, 3>) == 16);
static_assert(sizeof(inplace_array<StringView, 16>) == 16 * sizeof(StringView) + alignof(StringView));

static_assert(std::is_trivially_copyable_v<inplace_array<int, 4>>);
static_assert(std::is_trivially_copyable_v<inplace_array<with_default_member, 4>>);
static_assert(std::is_trivially_copyable_v<inplace_array<triple, 4>>);
static_assert(!std::is_trivially_copyable_v<inplace_array<counted<true>, 4>>);
static_assert(!std::is_trivially_copyable_v<inplace_array<String, 4>>);
static_assert(std::is_trivially_destructible_v<inplace_array<triple, 4>>);
static_assert(!std::is_trivially_destructible_v<inplace_array<counted<false>, 4>>);
static_assert(!std::is_trivially_destructible_v<inplace_array<String, 4>>);
static_assert(std::is_empty_v<inplace_array<String, 0>> && std::is_trivially_copyable_v<inplace_array<String, 0>>);
static_assert(std::is_copy_constructible_v<inplace_array<String, 4>> && std::is_copy_assignable_v<inplace_array<String, 4>>);
static_assert(!std::is_copy_constructible_v<inplace_array<Pointer<int>, 4>>);
static_assert(!std::is_copy_assignable_v<inplace_array<Pointer<int>, 4>>);
static_assert(std::is_nothrow_move_constructible_v<inplace_array<Pointer<int>, 4>>);
static_assert(!std::is_trivially_copy_constructible_v<user_provided_destructor<true>>);
static_assert(!std::is_trivially_copy_constructible_v<user_provided_destructor<false>>);
static_assert(!std::is_trivially_copy_constructible_v<inplace_array<user_provided_destructor<false>, 4>>);

static_assert(std::three_way_comparable<inplace_array<less_only, 4>>);
static_assert(std::equality_comparable<inplace_array<equality_comparable_only, 4>>);
static_assert(!std::three_way_comparable<inplace_array<equality_comparable_only, 4>>);
static_assert(!std::totally_ordered<inplace_array<equality_comparable_only, 4>>);

constexpr inplace_array<non_constexpr_default_ctor, 4> constant_empty_array;
static_assert(constant_empty_array.empty() && constant_empty_array.begin() == constant_empty_array.end());

constexpr inplace_array<triple, 3> constant_triples{2, 3};
static_assert(constant_triples.size() == 2 && constant_triples[1].c == 15);

template<typename T>
constexpr bool test_members()
{
    maker<T> make;
    {
        inplace_array<T, 6> a;
        fm_assert(a.empty() && a.size() == 0 && a.capacity() == 6 && a.max_size() == 6);
        fm_assert(a.begin() == a.end() && a.cbegin() == a.cend() && a.rbegin() == a.rend());

        const T two = make(2);
        fm_assert(value_of(a.push_back(two)) == 2);
        fm_assert(value_of(a.push_back(make(3))) == 3);
        fm_assert(value_of(make.call_with_ctor_args(5, [&](auto&&... args) -> T& { return a.emplace_back(args...); })) == 5);
        fm_assert(value_of(a.unchecked_push_back(make(7))) == 7);
        fm_assert(value_of(make.call_with_ctor_args(11, [&](auto&&... args) -> T& {
            return a.unchecked_emplace_back(args...);
        })) == 11);
        fm_assert(has_values(a, {2, 3, 5, 7, 11}) && make.has_live_count(6));

        {
            T* p = a.try_push_back(make(13));
            fm_assert(p == a.end() - 1 && value_of(*p) == 13 && a.size() == 6);
        }
        fm_assert(!a.try_push_back(make(17)) && !a.try_push_back(two));
        fm_assert(!make.call_with_ctor_args(19, [&](auto&&... args) { return a.try_emplace_back(args...); }));
        fm_assert(has_values(a, {2, 3, 5, 7, 11, 13}) && make.has_live_count(7));

        const auto& ca = a;
        fm_assert(value_of(a[1]) == 3 && value_of(a.at(5)) == 13 && value_of(a.front()) == 2 && value_of(a.back()) == 13);
        fm_assert(value_of(ca[2]) == 5 && value_of(ca.at(3)) == 7 && value_of(ca.front()) == 2 && value_of(ca.back()) == 13);
        fm_assert(a.data() == a.begin() && ca.data() == &ca[0] && ca.end() - ca.begin() == 6 && ca.cend() == ca.end());
        u64 code = 0, const_code = 0;
        for (auto it = a.rbegin(); it != a.rend(); ++it)
            code = code * 100 + u64(value_of(*it));
        for (auto it = ca.crbegin(); it != ca.crend(); ++it)
            const_code = const_code * 100 + u64(value_of(*it));
        fm_assert(code == 131107050302 && const_code == code);
        const ArrayView<T> view = a;
        const ArrayView<const T> const_view = ca;
        fm_assert(view.data() == a.data() && view.size() == 6 && const_view.data() == a.data() && const_view.size() == 6);

        a.pop_back();
        fm_assert(has_values(a, {2, 3, 5, 7, 11}) && make.has_live_count(6));
        T* q = a.erase(a.begin() + 1, a.begin() + 3);
        fm_assert(q == a.begin() + 1 && has_values(a, {2, 7, 11}) && make.has_live_count(4));
        q = a.insert(a.begin(), make(17));
        fm_assert(q == a.begin() && has_values(a, {17, 2, 7, 11}) && make.has_live_count(5));
        q = a.insert(a.begin() + 2, two);
        fm_assert(q == a.begin() + 2 && has_values(a, {17, 2, 2, 7, 11}) && make.has_live_count(6));
        q = a.insert(a.end(), make(19));
        fm_assert(q == a.end() - 1 && has_values(a, {17, 2, 2, 7, 11, 19}) && make.has_live_count(7));
        q = a.erase(a.begin() + 1);
        fm_assert(q == a.begin() + 1 && has_values(a, {17, 2, 7, 11, 19}) && make.has_live_count(6));
        q = make.call_with_ctor_args(23, [&](auto&&... args) { return a.emplace(a.begin() + 2, args...); });
        fm_assert(q == a.begin() + 2 && has_values(a, {17, 2, 23, 7, 11, 19}) && make.has_live_count(7));
        q = a.erase(a.begin());
        fm_assert(q == a.begin() && has_values(a, {2, 23, 7, 11, 19}) && make.has_live_count(6));
        q = a.erase(a.end() - 1);
        fm_assert(q == a.end() && has_values(a, {2, 23, 7, 11}) && make.has_live_count(5));
        q = a.erase(a.begin() + 1, a.begin() + 1);
        fm_assert(q == a.begin() + 1 && has_values(a, {2, 23, 7, 11}) && make.has_live_count(5));
        q = a.insert(a.begin() + 1, 2, make(29));
        fm_assert(q == a.begin() + 1 && has_values(a, {2, 29, 29, 23, 7, 11}) && make.has_live_count(7));

        a.resize(2, two);
        fm_assert(has_values(a, {2, 29}) && make.has_live_count(3));
        const T src[] = { make(31), make(37) };
        q = a.insert(a.begin() + 1, src, src + 2);
        fm_assert(q == a.begin() + 1 && has_values(a, {2, 31, 37, 29}) && make.has_live_count(7));
        q = a.insert(a.begin(), {make(41)});
        fm_assert(q == a.begin() && has_values(a, {41, 2, 31, 37, 29}) && make.has_live_count(8));
        a.erase(a.begin() + 1, a.end());
        q = a.insert_range(a.begin(), src);
        fm_assert(q == a.begin() && has_values(a, {31, 37, 41}) && make.has_live_count(6));
        a.append_range(src);
        fm_assert(has_values(a, {31, 37, 41, 31, 37}) && make.has_live_count(8));
        fm_assert(erase(a, make(37)) == 2);
        fm_assert(has_values(a, {31, 41, 31}) && make.has_live_count(6));
        fm_assert(erase_if(a, [](const T& x) { return value_of(x) == 31; }) == 2);
        fm_assert(has_values(a, {41}) && make.has_live_count(4));

        a.resize(3, two);
        fm_assert(has_values(a, {41, 2, 2}) && make.has_live_count(6));
        a.assign(2, make(43));
        fm_assert(has_values(a, {43, 43}) && make.has_live_count(5));
        a.assign(src, src + 2);
        fm_assert(has_values(a, {31, 37}) && make.has_live_count(5));
        a.assign({make(47), make(2)});
        fm_assert(has_values(a, {47, 2}) && make.has_live_count(5));
        a.assign_range(src);
        fm_assert(has_values(a, {31, 37}) && make.has_live_count(5));
        a = {make(3)};
        fm_assert(has_values(a, {3}) && make.has_live_count(4));
        a.clear();
        fm_assert(a.empty() && make.has_live_count(3));
        a.push_back(make(5));
        fm_assert(has_values(a, {5}) && make.has_live_count(4));

        if constexpr (std::is_default_constructible_v<T>)
        {
            a.resize(3);
            fm_assert(a.size() == 3 && value_of(a[0]) == 5 && value_of(a[2]) == value_of(T{}));
            const inplace_array<T, 4> b(2);
            fm_assert(b.size() == 2 && value_of(b[1]) == value_of(T{}));
        }
    }
    fm_assert(make.has_live_count(0));
    return true;
}

template<typename T>
constexpr bool test_copy_and_move()
{
    maker<T> make;
    {
        inplace_array<T, 5> a;
        a.push_back(make(2));
        a.push_back(make(3));
        a.push_back(make(5));
        inplace_array<T, 5> b = a;
        fm_assert(has_values(b, {2, 3, 5}) && a == b && make.has_live_count(6));
        inplace_array<T, 5> c = move(b);
        fm_assert(has_values(c, {2, 3, 5}) && b.size() == 3 && make.has_live_count(9));

        inplace_array<T, 5> d;
        d.push_back(make(7));
        d = a;
        fm_assert(has_values(d, {2, 3, 5}) && make.has_live_count(12));
        d = inplace_array<T, 5>{};
        fm_assert(d.empty() && make.has_live_count(9));
        d = move(c);
        fm_assert(has_values(d, {2, 3, 5}) && make.has_live_count(12));
        auto& alias = d;
        d = alias;
        fm_assert(has_values(d, {2, 3, 5}) && make.has_live_count(12));
        d = move(alias);
        fm_assert(has_values(d, {2, 3, 5}) && make.has_live_count(12));
        d = {make(11), make(13)};
        fm_assert(has_values(d, {11, 13}) && make.has_live_count(11));

        const inplace_array<T, 5> e(3, make(17));
        fm_assert(has_values(e, {17, 17, 17}) && make.has_live_count(14));
        const T src[] = { make(19), make(23) };
        const inplace_array<T, 5> f(src, src + 2);
        fm_assert(has_values(f, {19, 23}) && make.has_live_count(18));
        const inplace_array<T, 5> g{make(29), make(31), make(37)};
        fm_assert(has_values(g, {29, 31, 37}) && make.has_live_count(21));
    }
    fm_assert(make.has_live_count(0));
    return true;
}

template<typename T>
constexpr bool test_swap()
{
    maker<T> make;
    {
        inplace_array<T, 6> a{make(2), make(3), make(5), make(7)}, b{make(11)}, e;
        a.swap(b);
        fm_assert(has_values(a, {11}) && has_values(b, {2, 3, 5, 7}) && make.has_live_count(5));
        swap(a, b);
        fm_assert(has_values(a, {2, 3, 5, 7}) && has_values(b, {11}) && make.has_live_count(5));
        swap(b, e);
        fm_assert(b.empty() && has_values(e, {11}) && make.has_live_count(5));
        auto& alias = a;
        a.swap(alias);
        fm_assert(has_values(a, {2, 3, 5, 7}) && make.has_live_count(5));
    }
    fm_assert(make.has_live_count(0));
    return true;
}

template<typename T>
constexpr bool test_comparison()
{
    maker<T> make;
    {
        const inplace_array<T, 4> a{make(2), make(3)}, b{make(2), make(3)}, c{make(2), make(5)}, d{make(2)}, e;
        fm_assert(a == b && !(a != b) && a != c && a != d && d != e && e == inplace_array<T, 4>{});
        fm_assert(a < c && c > a && d < a && e < d && a <= b && a >= b && !(c < a));
        fm_assert(std::is_eq(a <=> b) && std::is_lt(a <=> c) && std::is_gt(a <=> d) && std::is_gt(d <=> e));
    }
    fm_assert(make.has_live_count(0));
    return true;
}

constexpr bool test_equality_comparable_only()
{
    using T = equality_comparable_only;
    const inplace_array<T, 4> a{T{2}, T{3}}, b = a, c{T{2}};
    fm_assert(a == b && a != c && c != inplace_array<T, 4>{});
    return true;
}

constexpr bool test_zero_capacity()
{
    inplace_array<triple, 0> a;
    fm_assert(a.empty() && a.size() == 0 && a.capacity() == 0 && a.max_size() == 0);
    fm_assert(a.begin() == a.end() && a.data() == nullptr && a.rbegin() == a.rend() && a.crbegin() == a.crend());
    fm_assert(!a.try_push_back(triple{2}) && !a.try_emplace_back(3));
    inplace_array<triple, 0> b = a;
    a = b;
    fm_assert(a == b && std::is_eq(a <=> b));
    a.swap(b);
    swap(a, b);
    a.clear();
    const triple src[] = { triple{5} };
    fm_assert(a.insert(a.begin(), 0, src[0]) == a.end());
    fm_assert(a.insert(a.begin(), src, src) == a.end());
    fm_assert(a.insert(a.begin(), std::initializer_list<triple>{}) == a.end());
    fm_assert(a.insert_range(a.end(), ArrayView<const triple>{}) == a.end());
    fm_assert(a.erase(a.begin(), a.end()) == a.end());
    a.append_range(ArrayView<const triple>{});
    a.assign(0, src[0]);
    a.assign(src, src);
    a.assign(std::initializer_list<triple>{});
    a.assign_range(ArrayView<const triple>{});
    a = std::initializer_list<triple>{};
    a.resize(0);
    a.resize(0, src[0]);
    fm_assert(erase(a, triple{7}) == 0 && erase_if(a, [](const triple&) { return true; }) == 0);
    const ArrayView<const triple> view = a;
    fm_assert(view.isEmpty() && a.empty());
    const inplace_array<triple, 0> c(0), d(0, src[0]), f(src, src), g{};
    fm_assert(c == d && f == g);
    return true;
}

constexpr bool test_rbegin_rend()
{
    using array = inplace_array<int, 5>;
    array a{2, 3, 5, 7, 11};
    const array& ca = a;
    fm_assert(a.rbegin().base() == a.end() && a.rend().base() == a.begin());
    fm_assert(ca.rbegin().base() == ca.end() && ca.rend().base() == ca.begin());
    fm_assert(ca.crbegin() == a.rbegin() && ca.crend() == a.rend() && a.rend() - a.rbegin() == 5);
    fm_assert(*a.rbegin() == 11 && *(a.rend() - 1) == 2);
    *a.rbegin() = 13;
    fm_assert(a.back() == 13);
    const inplace_array<triple, 2> t{triple{2}, triple{3}};
    fm_assert(t.rbegin()->c == 15 && (t.rbegin() + 1)->b == 6);
    return true;
}

static_assert(std::is_same_v<inplace_array<int, 5>::reverse_iterator, reverse_iterator<int*>>);
static_assert(std::is_same_v<inplace_array<int, 5>::const_reverse_iterator, reverse_iterator<const int*>>);
static_assert(std::is_same_v<inplace_array<int, 0>::reverse_iterator, reverse_iterator<int*>>);
static_assert(std::is_same_v<inplace_array<int, 0>::const_reverse_iterator, reverse_iterator<const int*>>);

static_assert(test_rbegin_rend());
static_assert(test_members<int>());
static_assert(test_members<with_default_member>());
static_assert(test_members<triple>());
static_assert(test_members<counted<true>>());
static_assert(test_members<counted<false>>());
static_assert(test_members<user_provided_destructor<true>>());
static_assert(test_members<user_provided_destructor<false>>());
static_assert(test_copy_and_move<int>());
static_assert(test_copy_and_move<with_default_member>());
static_assert(test_copy_and_move<triple>());
static_assert(test_copy_and_move<counted<true>>());
static_assert(test_copy_and_move<counted<false>>());
static_assert(test_copy_and_move<user_provided_destructor<true>>());
static_assert(test_copy_and_move<user_provided_destructor<false>>());
static_assert(test_swap<int>());
static_assert(test_swap<triple>());
static_assert(test_swap<counted<true>>());
static_assert(test_swap<counted<false>>());
static_assert(test_comparison<int>());
static_assert(test_comparison<with_default_member>());
static_assert(test_comparison<triple>());
static_assert(test_comparison<less_only>());
static_assert(test_comparison<counted<false>>());
static_assert(test_equality_comparable_only());
static_assert(test_zero_capacity());

String long_string(u32 i)
{
    const char digit = char('0' + i);
    return "a string too long for the small string buffer "_s + StringView{&digit, 1};
}

bool has_strings(const inplace_array<String, 5>& a, std::initializer_list<u32> expected)
{
    if (a.size() != expected.size())
        return false;
    else
    {
        u32 i = 0;
        for (u32 x : expected)
            if (a[i++] != long_string(x))
                return false;
        return true;
    }
}

void test_strings()
{
    inplace_array<String, 5> a;
    for (u32 i = 0; i < 4; i++)
        a.push_back(long_string(i));
    a.insert(a.begin() + 1, long_string(7));
    a.erase(a.begin() + 3);
    fm_assert(has_strings(a, {0, 7, 1, 3}));
    inplace_array<String, 5> b = a;
    b.pop_back();
    b.insert(b.begin(), 1, long_string(5));
    fm_assert(has_strings(b, {5, 0, 7, 1}));
    a.swap(b);
    fm_assert(has_strings(a, {5, 0, 7, 1}) && has_strings(b, {0, 7, 1, 3}));
    b = a;
    fm_assert(has_strings(b, {5, 0, 7, 1}) && a == b);
    a.resize(2);
    fm_assert(has_strings(a, {5, 0}) && a < b);
    a.clear();
    a.resize(1, long_string(9));
    fm_assert(has_strings(a, {9}));
}

void test_non_constexpr_default_ctor()
{
    inplace_array<non_constexpr_default_ctor, 4> a(2);
    a.push_back(non_constexpr_default_ctor{5});
    fm_assert(a.size() == 3 && a[0].v == 3 && a[1].v == 3 && a[2].v == 5);
}

void test_move_only()
{
    inplace_array<Pointer<int>, 4> a;
    a.push_back(Pointer<int>{InPlaceInit, 2});
    a.emplace_back(InPlaceInit, 3);
    a.insert(a.begin(), Pointer<int>{InPlaceInit, 5});
    a.emplace(a.begin() + 1, InPlaceInit, 7);
    a.erase(a.begin() + 2);
    fm_assert(a.size() == 3 && *a[0] == 5 && *a[1] == 7 && *a[2] == 3);
    inplace_array<Pointer<int>, 4> b = move(a);
    fm_assert(b.size() == 3 && *b[0] == 5 && *b[1] == 7 && *b[2] == 3 && !a[0]);
    a.clear();
    a.emplace_back(InPlaceInit, 11);
    swap(a, b);
    fm_assert(a.size() == 3 && b.size() == 1 && *a[2] == 3 && *b[0] == 11);
    a = move(b);
    fm_assert(a.size() == 1 && *a[0] == 11);
}

} // namespace

void Test::test_inplace_array()
{
    fm_assert(test_rbegin_rend());
    fm_assert(test_members<int>());
    fm_assert(test_members<with_default_member>());
    fm_assert(test_members<triple>());
    fm_assert(test_members<counted<true>>());
    fm_assert(test_members<counted<false>>());
    fm_assert(test_copy_and_move<int>());
    fm_assert(test_copy_and_move<triple>());
    fm_assert(test_copy_and_move<counted<true>>());
    fm_assert(test_copy_and_move<counted<false>>());
    fm_assert(test_copy_and_move<user_provided_destructor<false>>());
    fm_assert(test_swap<int>());
    fm_assert(test_swap<counted<false>>());
    fm_assert(test_comparison<less_only>());
    fm_assert(test_comparison<counted<true>>());
    fm_assert(test_equality_comparable_only());
    fm_assert(test_zero_capacity());
    test_strings();
    test_non_constexpr_default_ctor();
    test_move_only();
}

} // namespace floormat
