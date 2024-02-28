#include "app.hpp"
#include "compat/intrusive-ptr.hpp"
#include "compat/defs.hpp"

#ifdef __CLION_IDE__
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "performance-unnecessary-copy-initialization"
#endif

namespace floormat {

namespace { struct Test2 { int val = 0; uint32_t _counter = 0; }; }

template<>
constexpr auto
iptr::refcount_access<iptr::non_atomic_u32_tag, Test2>::access(Test2* ptr) noexcept -> counter_type&
{
    return ptr->_counter;
}

namespace {

struct non_copyable
{
    int value = 0;
    explicit constexpr non_copyable(DirectInitT) {}
    explicit constexpr non_copyable(DirectInitT, int value) : value{value} {}
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT(non_copyable);
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(non_copyable);
};

struct S { // todo fm_assert_equal
    int instances;
    unsigned allocs, frees;
    bool operator==(const S&) const = default;
} s = {};

struct Test1
{
    friend struct iptr::refcount_access<iptr::non_atomic_u32_tag, Test1>;
    Test1(non_copyable nc) noexcept : value{nc.value} { s.instances++; s.allocs++; }
    ~Test1() noexcept { s.instances--; fm_assert(s.instances >= 0); s.frees++; }

    int value = 0;

private:
    iptr::refcount_traits<iptr::non_atomic_u32_tag, Test1>::counter_type counter{0};
};

} // namespace

template<>
constexpr auto iptr::refcount_access<iptr::non_atomic_u32_tag, Test1>::access(Test1* ptr) noexcept -> counter_type&
{
    return ptr->counter;
}

template class basic_iptr<iptr::non_atomic_u32_tag, Test1>;

namespace {

void test_copy()
{
    using myptr = local_iptr<Test1>;

    s = {};

    { (void)myptr{nullptr};
      fm_assert(s == S{});
      (void)myptr{};
      fm_assert(s == S{});
      { Test1 t1{non_copyable{DirectInit}}; }
      fm_assert(s != S{});
      fm_assert(s == S{0, 1, 1});

      { Test1 t1{non_copyable{DirectInit}};
        fm_assert(s == S{1, 2, 1});
      } fm_assert(s == S{0, 2, 2});
    }

    { auto a = myptr{InPlaceInit, non_copyable{DirectInit}};
      fm_assert(s == S{1, 3, 2});
      auto b = a;
      fm_assert(s == S{1, 3, 2});
      fm_assert(b.get() == a.get());
    } fm_assert(s == S{0, 3, 3});

    { auto a = myptr{InPlaceInit, non_copyable{DirectInit}};
      fm_assert(s == S{1, 4, 3});
      a = {};
      fm_assert(s == S{0, 4, 4});
    } fm_assert(s == S{0, 4, 4});

    { auto a = myptr{InPlaceInit, non_copyable{DirectInit, 1}};
      auto b = myptr{InPlaceInit, non_copyable{DirectInit, 2}};
      { auto c = myptr{InPlaceInit, non_copyable{DirectInit, 3}};
        auto d = myptr{InPlaceInit, non_copyable{DirectInit, 4}};
      }
      fm_assert(a.use_count() == 1);
      fm_assert(a->value == 1);
      fm_assert(b->value == 2);
      fm_assert(s == S{2, 8, 6});
      b = a;
      fm_assert(b.use_count() == 2);
      fm_assert(a.use_count() == 2);
      b = nullptr;
      fm_assert(b.use_count() == 0);
      fm_assert(a.use_count() == 1);
      fm_assert(s == S{1, 8, 7});
      fm_assert(a->value == 1);
    } fm_assert(s == S{0, 8, 8});
}

#define fm_assert_free() do { fm_assert(s.allocs == s.frees); fm_assert(s.instances == 0); } while (false)

void test_move()
{
    using myptr = local_iptr<Test1>;

    fm_assert_free();
    s = {};

    { auto a = myptr{InPlaceInit, non_copyable{DirectInit, 1}};
      auto b = myptr{InPlaceInit, non_copyable{DirectInit, 2}};
      fm_assert(s == S{2, 2, 0});
      { auto c = myptr{InPlaceInit, non_copyable{DirectInit, 3}};
        auto d = myptr{InPlaceInit, non_copyable{DirectInit, 4}};
        fm_assert(s == S{4, 4, 0});
      } fm_assert(s == S{2, 4, 2});

      { a = move(b);
        fm_assert(s == S{1, 4, 3});
        fm_assert(!b);
        fm_assert(a);
        fm_assert(b.use_count() == 0);
        fm_assert(a.use_count() == 1);
      }
    } fm_assert(s == S{0, 4, 4});
}

constexpr bool test_cexpr() // todo
{
    using myptr = local_iptr<Test2>;

    // construct
    auto foo1 = myptr{};
    auto foo2 = myptr{nullptr};

    fm_assert(foo1.use_count() == 0);
    fm_assert(foo2.use_count() == 0);
    foo1 = move(foo2);
    fm_assert(foo1.use_count() == 0);
    fm_assert(foo2.use_count() == 0);

    return true;
}

} // namespace

void test_app::test_iptr()
{
    static_assert(test_cexpr());
    test_copy();
    test_move();
}

} // namespace floormat
