#include "app.hpp"
#include "compat/hash.hpp"
#include "compat/array-size.hpp"
#include "src/global-coords.hpp"
#include "src/world.hpp"
#include <cr/StringView.h>
#include <bitset>
#include <memory>
#include <mg/Functions.h>

namespace floormat {

namespace {

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
    constexpr size_t BucketCount = Math::max(world::initial_capacity, size_t(Math::ceil(Iters / world::max_load_factor)));
    uint32_t num_collisions = 0, iters = 0;
    auto bitset_ = std::make_unique<std::bitset<BucketCount>>(false);
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

//#define FM_TEST_DEBUG_PRINT

    if (num_collisions > MaxCollisions)
    {
        DBG_nospace << Debug::color(Debug::Color::Magenta)
                    << "fail" << Debug::resetColor << ": test/hash(\"" << name << "\")"
                    << " collisions " << num_collisions << " > " << MaxCollisions
                    << " (iters:" << iters << " buckets:" << BucketCount << ")";
#ifdef FM_TEST_DEBUG_PRINT
        return false;
#else
        fm_abort("test/hash: too many collisions");
#endif
    }
    else if (num_collisions != MaxCollisions)
    {
#ifdef FM_TEST_DEBUG_PRINT
        DBG_nospace << Debug::color(Debug::Color::Cyan) << "pass" << Debug::resetColor
                    << ": test/hash(\"" << name << "\") collisions "
                    << num_collisions << " <= " << MaxCollisions
                    << " (iters:" << iters << " buckets:" << BucketCount << ")";
#endif
    }
    return true;
}

} // namespace

void Test::test_hash()
{
    test_simple();

    bool success = true;

#ifdef FM_TEST_DEBUG_PRINT
    DBG_nospace << "";
#endif

#ifdef FM_TEST_DEBUG_PRINT
#define FM_MAYBE_FAIL void()
#else
#define FM_MAYBE_FAIL FM_MAYBE_FAIL_
#endif
#define FM_MAYBE_FAIL_ do { if (!success) { DBG_nospace << ""; fm_abort("test/hash: too many collisions"); } } while (false)

    success &= test_collisions<  2, 1,    1 >("small1");    FM_MAYBE_FAIL;
    success &= test_collisions<  3, 0,    1 >("small2");    FM_MAYBE_FAIL;
    success &= test_collisions<  4, 0,    1 >("small3");    FM_MAYBE_FAIL;
    success &= test_collisions<  5, 0,    1 >("small4");    FM_MAYBE_FAIL;
    success &= test_collisions<  6, 0,    4 >("mid1");      FM_MAYBE_FAIL;
    success &= test_collisions<  7, 0,    7 >("mid2");      FM_MAYBE_FAIL;
    success &= test_collisions<  8, 0,   10 >("mid3");      FM_MAYBE_FAIL;
    success &= test_collisions<  9, 0,   15 >("mid4");      FM_MAYBE_FAIL;
    success &= test_collisions< 10, 0,   22 >("large1");    FM_MAYBE_FAIL;
    success &= test_collisions< 12, 0,   51 >("large2");    FM_MAYBE_FAIL;
    success &= test_collisions< 14, 0,   94 >("large3");    FM_MAYBE_FAIL;
    success &= test_collisions< 15, 0,  118 >("large4");    FM_MAYBE_FAIL;
    success &= test_collisions< 16, 0,  107 >("large5");    FM_MAYBE_FAIL;
    success &= test_collisions<  3, 1,    2 >("zsmall1");   FM_MAYBE_FAIL;
    success &= test_collisions<  4, 1,    3 >("zsmall2");   FM_MAYBE_FAIL;
    success &= test_collisions<  2, 3,    1 >("zmid1");     FM_MAYBE_FAIL;
    success &= test_collisions<  3, 3,    7 >("zmid1");     FM_MAYBE_FAIL;
    success &= test_collisions<  2, 6,    4 >("zmid2");     FM_MAYBE_FAIL;
    success &= test_collisions<  2, 10,  14 >("zmid3");     FM_MAYBE_FAIL;
    success &= test_collisions<  6, 2,   35 >("zmid4");     FM_MAYBE_FAIL;
    success &= test_collisions<  7, 1,   28 >("zmid5");     FM_MAYBE_FAIL;
    success &= test_collisions< 10, 1,   89 >("zlarge1");   FM_MAYBE_FAIL;
    success &= test_collisions< 10, 4,  274 >("zlarge2");   FM_MAYBE_FAIL;
    success &= test_collisions< 16, 8, 1126 >("zlarge3");   FM_MAYBE_FAIL;
    success &= test_collisions< 32, 8, 4397 >("zlarge4");   FM_MAYBE_FAIL;

    (void)success;
    //FM_MAYBE_FAIL_;
}

} // namespace floormat
