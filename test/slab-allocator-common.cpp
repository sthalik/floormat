#include "slab-allocator-common.hpp"
#include "compat/assert.hpp"
#include <cr/ArrayView.h>
#if fm_ASAN
#include <sanitizer/allocator_interface.h>
#include <sanitizer/asan_interface.h>
#endif

namespace floormat::slab_test {

namespace {

constexpr u64 stamp_multiplier = 0x9e3779b97f4a7c15;

} // namespace

#if fm_ASAN
bool is_poisoned(const void* p) { return __asan_address_is_poisoned(p) != 0; }
bool is_owned_by_asan_allocator(const void* p) { return __sanitizer_get_ownership(p) != 0; }
#endif

void write_stamp(stamped_buffer& b, u32 serial)
{
    b.words[0] = serial;
    for (u32 i = 1; i < 8; i++)
        b.words[i] = serial * stamp_multiplier + i;
}

u32 read_stamp(const stamped_buffer& b)
{
    for (u32 i = 1; i < 8; i++)
        fm_assert(b.words[i] == b.words[0] * stamp_multiplier + i);
    return (u32)b.words[0];
}

void write_stamp(stamped_12_byte_buffer& b, u32 serial)
{
    b.words[0] = serial;
    for (u32 i = 1; i < 3; i++)
        b.words[i] = (u32)(serial * stamp_multiplier + i);
}

u32 read_stamp(const stamped_12_byte_buffer& b)
{
    for (u32 i = 1; i < 3; i++)
        fm_assert(b.words[i] == (u32)(b.words[0] * stamp_multiplier + i));
    return b.words[0];
}

void write_pattern(char* p, u32 bytes, u32 serial)
{
    for (u32 i = 0; i < bytes; i++)
        p[i] = (char)(serial * 131 + i * 7);
}

bool has_pattern(const char* p, u32 bytes, u32 serial)
{
    for (u32 i = 0; i < bytes; i++)
        if (p[i] != (char)(serial * 131 + i * 7))
            return false;
    return true;
}

void shuffle(ArrayView<u32> values, u64 seed)
{
    u64 x = seed;
    for (u32 k = (u32)values.size(); k-- > 1; )
    {
        x = x * 6364136223846793005 + 1442695040888963407;
        const u32 j = (u32)((x >> 32) % (k + 1));
        const u32 tmp = values[k];
        values[k] = values[j];
        values[j] = tmp;
    }
}

} // namespace floormat::slab_test
