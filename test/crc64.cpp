#include "app.hpp"
#include "compat/crc64.hpp"
#include <cr/StringView.h>
#include <bit>

namespace floormat::Test {

namespace {

u64 crc64(u64 init, StringView str)
{
    u64 crc = init;
    crc = Hash::crc64_update(crc, str.data(), str.size());

    return crc;
}

} // namespace

void test_crc64()
{
    {
        constexpr u64 expected =  0x6C40DF5F0B497347;

        {u64 crc = Hash::CRC64_INITIALIZER;
         crc = crc64(crc, "123456789"_s);
         fm_assert(crc == expected);}

        {u64 crc = Hash::CRC64_INITIALIZER;
         crc = crc64(crc, "123"_s);
         crc = crc64(crc, "456789"_s);
         fm_assert(crc == expected);}
    }
}

} // namespace floormat::Test
