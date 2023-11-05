#include "app.hpp"
#include "compat/int-hash.hpp"
#include <Corrade/Containers/StringView.h>
#include <bitset>
#include <memory>

namespace floormat {

namespace {

void test_simple()
{
    constexpr StringView list[] = { "foo"_s, "bar"_s, "bar\0"_s, "bar2"_s, "baz"_s, };
    constexpr auto size = arraySize(list);

    size_t hashes[size] = {};
    for (auto i = 0uz; i < size; i++)
        hashes[i] = hash_64(list[i].data(), list[i].size());

    for (auto i = 0uz; i < size; i++)
        for (auto j = i+1; j < size; j++)
        {
            if (hashes[i] == hashes[j])
                fm_abort("hash collision between '%s' and '%s': 0x%p", list[i].data(), list[j].data(), (void*)hashes[i]);
        }
}

void test_collisions()
{
    constexpr int max = 8;
    constexpr size_t size = (2*max+1)*(2*max+1)*4 * 10;
    constexpr size_t max_collisions = size*2/100;
    size_t iter = 0, num_collisions = 0;
    auto bitset_ = std::make_unique<std::bitset<size>>(false);
    auto& bitset = *bitset_;

    for (int j = -max; j <= max; j++)
    {
        for (int i = -max; i <= max; i++)
        {
            for (int k = -2; k <= 1; k++)
            {
                ++iter;
                uint64_t value = 0;
                value |= (uint64_t)(uint16_t)(int16_t)i << 0;
                value |= (uint64_t)(uint16_t)(int16_t)j << 16;
                value |= (uint64_t)(uint16_t)(int16_t)k << 32;
                auto x = (size_t)hash_int(value);
                //Debug {} << "bitset:" << i << j << k << "=" << x % size << "/" << x;
                x %= size;
#if 1
                if (bitset.test(x) && ++num_collisions >= max_collisions)
                    fm_abort("test/bitset: %zu collisions at iter %zu id %zu (%d;%d:%d)", num_collisions, iter, x, i, j, k);
#else
                if ((void)max_collisions, bitset.test(x))
                        fm_warn("test/bitset: %zu collisions at iter %zu id %zu (%d;%d:%d)", ++num_collisions, iter, x, i, j, k);
#endif
                bitset.set(x);
            }
        }
    }
}

} // namespace

void test_app::test_hash()
{
    test_simple();
    test_collisions();
}

} // namespace floormat
