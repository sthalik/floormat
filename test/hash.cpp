#include "app.hpp"
#include "compat/hash.hpp"
#include "compat/array-size.hpp"
#include "src/global-coords.hpp"
#include "src/world.hpp"
#include <cr/StringView.h>
#include <cr/Pointer.h>
#include <bitset>
#include <mg/Functions.h>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wunused-macros"
#endif

//#define FM_TEST_DEBUG_PRINT
//#define FM_TEST_DEBUG_VERBOSE

namespace floormat {

namespace {

bool first = true;

void test_simple()
{
    constexpr StringView list[] = {
        "foo"_s, "bar"_s, "bar\0"_s, "bar2"_s, "baz"_s,
        "The quick brown fox jumps over the lazy dog"_s,
        "The quick brown fox jumps over the lazy cog"_s,
    };
    constexpr auto size = array_size(list);

    size_t hashes[size] = {};
    for (auto i = 0uz; i < size; i++)
        hashes[i] = hash_buf(list[i].data(), list[i].size());

    for (auto i = 0uz; i < size; i++)
        for (auto j = i+1; j < size; j++)
        {
            if (hashes[i] == hashes[j])
                fm_abort("hash collision between '%s' and '%s': 0x%p",
                    list[i].data(), list[j].data(), (void*)hashes[i]);
        }
}

template<int16_t CoordMax, int8_t ZMax, uint32_t MaxCollisions>
[[nodiscard]] bool test_collisions(const char* name)
{
    constexpr size_t Iters = (CoordMax*2 + 1)*(CoordMax*2 + 1)*(1+ZMax); // NOLINT(*-implicit-widening-of-multiplication-result)
    constexpr size_t BucketCount = Math::max(world::initial_capacity, size_t(Math::ceil(Iters / 0.2f)));
    uint32_t num_collisions = 0, iters = 0;
    auto bitset_ = Pointer<std::bitset<BucketCount>>{InPlace, false};
    auto& bitset = *bitset_;

    for (int16_t j = -CoordMax; j <= CoordMax; j++)
        for (int16_t i = -CoordMax; i <= CoordMax; i++)
            for (int8_t k = 0; k <= ZMax; k++)
            {
                auto p = global_coords{i, j, k};
                auto h = p.hash();
                h %= BucketCount;
                auto ref = bitset[h];
                if (ref) [[unlikely]]
                    ++num_collisions;
                else
                    ref = true;
                (void)ref;
                iters++;
            }

    fm_assert_equal(Iters, iters);

    constexpr auto print = [](Debug::Color c, StringView status, StringView name, StringView op,
                              uint32_t num_collisions, uint32_t iters)
    {
        if (first)
        {
            DBG_nospace << "";
        }
        first = false;
        DBG_nospace << Debug::color(c) << status << Debug::resetColor
                    << ": " << name << "\t"
                    << num_collisions << "\t" << op << "\t" << MaxCollisions
                    << "\t\t(iters:" << iters << " buckets:" << BucketCount << ")";
    };

#define FM_FAIL fm_abort("test/hash: too many collisions")
#ifdef FM_TEST_DEBUG_PRINT
#define FM_MAYBE_FAIL void()
#else
#define FM_MAYBE_FAIL FM_MAYBE_FAIL_
#endif
#define FM_MAYBE_FAIL_ do { if (!success) { DBG_nospace << ""; FM_FAIL; } } while (false)

    if (num_collisions > MaxCollisions)
    {
        print(Debug::Color::Magenta, "fail", name, "!",  num_collisions, iters);
#ifdef FM_TEST_DEBUG_PRINT
        return false;
#else
        FM_FAIL;
#endif
    }
    else if (num_collisions < MaxCollisions)
    {
#if defined FM_TEST_DEBUG_PRINT && defined FM_TEST_DEBUG_VERBOSE
        print(Debug::Color::Cyan, "pass", name, " ", num_collisions, iters);
#endif
    }
    return true;
}

#if !defined FM_TEST_DEBUG_PRINT && defined FM_TEST_DEBUG_VERBOSE
#error assertion failed: !defined FM_TEST_DEBUG_PRINT && defined FM_TEST_DEBUG_VERBOSE
#endif

} // namespace

void Test::test_hash()
{
    test_simple();

    bool success = true;
    first = true;

    // best result is xxHash:
    success &= test_collisions<  2, 1,    1 >("small1 ");   FM_MAYBE_FAIL;
    success &= test_collisions<  3, 0,    2 >("small2 ");   FM_MAYBE_FAIL;
    success &= test_collisions<  4, 0,    2 >("small3 ");   FM_MAYBE_FAIL;
    success &= test_collisions<  5, 0,    1 >("small4 ");   FM_MAYBE_FAIL;
    success &= test_collisions<  6, 0,    4 >("mid1   ");   FM_MAYBE_FAIL;
    success &= test_collisions<  7, 0,    7 >("mid2   ");   FM_MAYBE_FAIL;
    success &= test_collisions<  8, 0,   10 >("mid3   ");   FM_MAYBE_FAIL;
    success &= test_collisions<  9, 0,   15 >("mid4   ");   FM_MAYBE_FAIL;
    success &= test_collisions< 10, 0,   22 >("large1 ");   FM_MAYBE_FAIL;
    success &= test_collisions< 12, 0,   51 >("large2 ");   FM_MAYBE_FAIL;
    success &= test_collisions< 14, 0,   94 >("large3 ");   FM_MAYBE_FAIL;
    success &= test_collisions< 15, 0,  118 >("large4 ");   FM_MAYBE_FAIL;
    success &= test_collisions< 16, 0,  110 >("large5 ");   FM_MAYBE_FAIL;
    success &= test_collisions<  3, 1,    2 >("zsmall1");   FM_MAYBE_FAIL;
    success &= test_collisions<  4, 1,    3 >("zsmall2");   FM_MAYBE_FAIL;
    success &= test_collisions<  2, 3,    1 >("zmid1"  );   FM_MAYBE_FAIL;
    success &= test_collisions<  3, 3,    7 >("zmid1"  );   FM_MAYBE_FAIL;
    success &= test_collisions<  2, 6,    4 >("zmid2"  );   FM_MAYBE_FAIL;
    success &= test_collisions<  2, 10,  14 >("zmid3"  );   FM_MAYBE_FAIL;
    success &= test_collisions<  6, 2,   35 >("zmid4"  );   FM_MAYBE_FAIL;
    success &= test_collisions<  7, 1,   28 >("zmid5"  );   FM_MAYBE_FAIL;
    success &= test_collisions< 10, 1,   89 >("zlarge1");   FM_MAYBE_FAIL;
    success &= test_collisions< 10, 4,  274 >("zlarge2");   FM_MAYBE_FAIL;
    success &= test_collisions< 16, 8, 1126 >("zlarge3");   FM_MAYBE_FAIL;
    success &= test_collisions< 32, 8, 4397 >("zlarge4");   FM_MAYBE_FAIL;

    (void)success;
    FM_MAYBE_FAIL_;
}

} // namespace floormat
