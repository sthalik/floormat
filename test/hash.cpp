#include "app.hpp"
#include "compat/int-hash.hpp"
#include <bitset>
#include <memory>

namespace floormat {

void test_app::test_hash()
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
                auto x = (size_t)int_hash(value);
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

} // namespace floormat
