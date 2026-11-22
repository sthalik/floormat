#include "app.hpp"
#include "compat/reverse-iterator.hpp"
#include "compat/move.hpp"

namespace floormat {

namespace {

struct int_pair
{
    int a, b;
};

struct bidirectional_int_iterator
{
    using value_type = int;
    using difference_type = isize;

    int* p = nullptr;
    int* num_iter_move_calls = nullptr;
    int* num_iter_swap_calls = nullptr;

    constexpr int& operator*() const { return *p; }
    constexpr int* operator->() const { return p; }
    constexpr bidirectional_int_iterator& operator++() { ++p; return *this; }
    [[maybe_unused]] constexpr bidirectional_int_iterator operator++(int) { auto old = *this; ++p; return old; }
    constexpr bidirectional_int_iterator& operator--() { --p; return *this; }
    [[maybe_unused]] constexpr bidirectional_int_iterator operator--(int) { auto old = *this; --p; return old; }
    constexpr bool operator==(const bidirectional_int_iterator& o) const { return p == o.p; }
};

constexpr isize operator-(const bidirectional_int_iterator& a, const bidirectional_int_iterator& b) { return a.p - b.p; }

constexpr int&& iter_move(const bidirectional_int_iterator& i)
{
    ++*i.num_iter_move_calls;
    return move(*i.p);
}

constexpr void iter_swap(const bidirectional_int_iterator& a, const bidirectional_int_iterator& b)
{
    ++*a.num_iter_swap_calls;
    swap(*a.p, *b.p);
}

struct random_access_int_iterator
{
    using value_type = int;
    using difference_type = isize;

    int* p = nullptr;

    constexpr int& operator*() const { return *p; }
    constexpr int& operator[](isize n) const { return p[n]; }
    constexpr random_access_int_iterator& operator++() { ++p; return *this; }
    [[maybe_unused]] constexpr random_access_int_iterator operator++(int) { auto old = *this; ++p; return old; }
    constexpr random_access_int_iterator& operator--() { --p; return *this; }
    [[maybe_unused]] constexpr random_access_int_iterator operator--(int) { auto old = *this; --p; return old; }
    constexpr random_access_int_iterator& operator+=(isize n) { p += n; return *this; }
    constexpr random_access_int_iterator& operator-=(isize n) { p -= n; return *this; }
    constexpr random_access_int_iterator operator+(isize n) const { return {p + n}; }
    constexpr random_access_int_iterator operator-(isize n) const { return {p - n}; }
    constexpr isize operator-(const random_access_int_iterator& o) const { return p - o.p; }
    constexpr auto operator<=>(const random_access_int_iterator&) const = default;
};

[[maybe_unused]] constexpr random_access_int_iterator operator+(isize n, const random_access_int_iterator& i)
{
    return i + n;
}

} // namespace

} // namespace floormat

namespace std {

template<>
constexpr inline bool disable_sized_sentinel_for<floormat::bidirectional_int_iterator,
                                                 floormat::bidirectional_int_iterator> = true;

} // namespace std

namespace floormat {

namespace {

template<typename T> concept has_arrow_operator = requires(const T& x) { x.operator->(); };
template<typename T>
concept has_relational_operator = requires(const T& x) { x < x; } || requires(const T& x) { x > x; } ||
                                  requires(const T& x) { x <= x; } || requires(const T& x) { x >= x; };

using bidirectional_reverse_iterator = reverse_iterator<bidirectional_int_iterator>;
using random_access_reverse_iterator = reverse_iterator<random_access_int_iterator>;

static_assert(sizeof(reverse_iterator<int*>) == sizeof(int*));
static_assert(std::is_trivially_copyable_v<reverse_iterator<int*>>);
static_assert(std::is_same_v<reverse_iterator<const int*>::value_type, int>);
static_assert(std::is_same_v<reverse_iterator<const int*>::reference, const int&>);
static_assert(std::is_same_v<reverse_iterator<const int*>::pointer, const int*>);
static_assert(std::is_convertible_v<reverse_iterator<int*>, reverse_iterator<const int*>>);
static_assert(!std::is_convertible_v<reverse_iterator<const int*>, reverse_iterator<int*>>);
static_assert(std::is_assignable_v<reverse_iterator<const int*>&, const reverse_iterator<int*>&>);
static_assert(!std::is_assignable_v<reverse_iterator<int*>&, const reverse_iterator<const int*>&>);

static_assert(std::random_access_iterator<reverse_iterator<int*>>);
static_assert(!std::contiguous_iterator<reverse_iterator<int*>>);
static_assert(std::is_same_v<reverse_iterator<int*>::iterator_concept, std::random_access_iterator_tag>);
static_assert(std::is_same_v<reverse_iterator<int*>::iterator_category, std::random_access_iterator_tag>);
static_assert(std::sized_sentinel_for<reverse_iterator<const int*>, reverse_iterator<int*>>);
static_assert(std::is_same_v<decltype(std::ranges::iter_move(std::declval<const reverse_iterator<int*>&>())), int&&>);

static_assert(std::bidirectional_iterator<bidirectional_int_iterator>);
static_assert(std::bidirectional_iterator<bidirectional_reverse_iterator>);
static_assert(!std::random_access_iterator<bidirectional_reverse_iterator>);
static_assert(std::is_same_v<bidirectional_reverse_iterator::iterator_concept, std::bidirectional_iterator_tag>);
static_assert(std::is_same_v<bidirectional_reverse_iterator::iterator_category, std::bidirectional_iterator_tag>);
static_assert(std::is_same_v<bidirectional_reverse_iterator::pointer, int*>);
static_assert(!has_relational_operator<bidirectional_reverse_iterator>);
static_assert(!std::sized_sentinel_for<bidirectional_int_iterator, bidirectional_int_iterator>);
static_assert(!std::sized_sentinel_for<bidirectional_reverse_iterator, bidirectional_reverse_iterator>);

static_assert(std::random_access_iterator<random_access_int_iterator>);
static_assert(std::random_access_iterator<random_access_reverse_iterator>);
static_assert(std::is_same_v<random_access_reverse_iterator::iterator_concept, std::random_access_iterator_tag>);
static_assert(std::is_same_v<random_access_reverse_iterator::iterator_category, std::random_access_iterator_tag>);
static_assert(std::is_same_v<random_access_reverse_iterator::pointer, void>);
static_assert(!has_arrow_operator<random_access_reverse_iterator>);

constexpr bool test_arithmetic()
{
    using iterator = reverse_iterator<int*>;
    int a[5] = {2, 3, 5, 7, 11};
    const iterator first{a + 5}, last{a};
    iterator r = first;
    fm_assert(*r == 11 && r[1] == 7 && *(r + 2) == 5 && *(2 + r) == 5 && r.base() == a + 5);
    fm_assert(last - first == 5 && first - last == -5);
    fm_assert(*(last - 1) == 2 && (last - 1).base() == a + 1);
    r += 3;
    fm_assert(*r == 3);
    r -= 2;
    fm_assert(*r == 7);
    fm_assert(*r++ == 7 && *r == 5 && *r-- == 5 && *r == 7);
    fm_assert(*++r == 5 && *--r == 7);
    *r = 13;
    fm_assert(a[3] == 13);
    fm_assert(iterator{} == iterator{} && iterator{}.base() == nullptr);
    fm_assert(make_reverse_iterator(a + 5) == first);
    return true;
}

constexpr bool test_comparison()
{
    using const_iterator = reverse_iterator<const int*>;
    int a[5] = {2, 3, 5, 7, 11};
    const reverse_iterator<int*> r{a + 4};
    const const_iterator c = r, first{a + 5}, last{a};
    fm_assert(*c == 7 && c == r && r == c && c != first && r != last);
    fm_assert(c < last && first < c && c > first && c <= r && c >= r && r < last);
    fm_assert(std::is_eq(c <=> r) && std::is_lt(first <=> c) && std::is_gt(last <=> r));
    fm_assert(last - c == 4 && r - first == 1);
    const_iterator d;
    d = r;
    fm_assert(d == r && *d == 7);
    return true;
}

constexpr bool test_member_access()
{
    const int_pair p[2] = {{2, 3}, {5, 7}};
    const reverse_iterator<const int_pair*> r{p + 2};
    fm_assert(r->a == 5 && (r + 1)->b == 3 && r[1].a == 2);
    return true;
}

constexpr bool test_bidirectional_base()
{
    int a[3] = {2, 3, 5};
    int num_iter_move_calls = 0, num_iter_swap_calls = 0;
    const bidirectional_int_iterator first{a, &num_iter_move_calls, &num_iter_swap_calls};
    const bidirectional_int_iterator last{a + 3, &num_iter_move_calls, &num_iter_swap_calls};
    const bidirectional_reverse_iterator r{last}, rend = make_reverse_iterator(first);
    int values[3] = {};
    u32 n = 0;
    for (auto i = r; i != rend; ++i)
        values[n++] = *i;
    fm_assert(n == 3 && values[0] == 5 && values[1] == 3 && values[2] == 2);
    fm_assert(r.operator->() == a + 2 && rend - r == 3 && r - rend == -3);
    fm_assert(std::ranges::iter_move(r) == 5 && num_iter_move_calls == 1);
    auto r2 = r;
    ++r2;
    ++r2;
    std::ranges::iter_swap(r, r2);
    fm_assert(num_iter_swap_calls == 1 && a[0] == 5 && a[2] == 2);
    --r2;
    auto r3 = r;
    ++r3;
    fm_assert(*r2 == 3 && r2 != r && r2 == r3);
    return true;
}

constexpr bool test_random_access_base()
{
    int a[4] = {2, 3, 5, 7};
    const random_access_reverse_iterator r{random_access_int_iterator{a + 4}};
    fm_assert(*r == 7 && r[2] == 3 && *(r + 3) == 2 && *(1 + r) == 5);
    fm_assert(r < r + 1 && r + 1 > r && std::is_gt(r + 1 <=> r) && (r + 4) - r == 4);
    auto i = r;
    i += 3;
    fm_assert(*i == 2 && i == r + 3);
    i -= 2;
    --i;
    fm_assert(i == r && *i == 7);
    return true;
}

static_assert(test_arithmetic());
static_assert(test_comparison());
static_assert(test_member_access());
static_assert(test_bidirectional_base());
static_assert(test_random_access_base());

} // namespace

void Test::test_reverse_iterator()
{
    fm_assert(test_arithmetic());
    fm_assert(test_comparison());
    fm_assert(test_member_access());
    fm_assert(test_bidirectional_base());
    fm_assert(test_random_access_base());
}

} // namespace floormat
