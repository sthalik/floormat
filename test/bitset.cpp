#include "app.hpp"
#include "compat/bitset.hpp"

namespace floormat {

namespace {

static_assert(sizeof(bitset<8>) == 1);
static_assert(sizeof(bitset<9>) == 4);
static_assert(sizeof(bitset<32>) == 4);
static_assert(sizeof(bitset<33>) == 8);

template<usize N>
constexpr bool check()
{
    bitset<N> b;
    const auto& cb = b;
    if (cb.any())
        return false;

    for (auto i = 0uz; i < N; i++)
    {
        b[i] = true;
        if (!cb[i] || !cb.any())
            return false;
        if (i > 0 && b[i - 1])
            return false;
        if (i + 1 < N && b[i + 1])
            return false;
        b[i] = false;
        if (cb[i] || cb.any())
            return false;
    }

    b[N - 1] = true;
    b[0] = b[N - 1];
    if (!cb[0])
        return false;
    b.reset();
    if (cb.any())
        return false;

    if constexpr (N < sizeof(usize)*8)
        if (bitset<N>{1uz << N}.any())
            return false;

    return true;
}

constexpr bool check_full_word()
{
    const bitset<sizeof(usize)*8> b{~0uz};
    for (auto i = 0uz; i < sizeof(usize)*8; i++)
        if (!b[i])
            return false;
    return true;
}

constexpr bool check_all()
{
    return check<1>() && check<8>() && check<9>() && check<32>() && check<33>() &&
           check<63>() && check<64>() && check<65>() && check<129>() && check_full_word();
}
static_assert(check_all());

} // namespace

void Test::test_bitset()
{
    fm_assert(check_all());
}

} // namespace floormat
