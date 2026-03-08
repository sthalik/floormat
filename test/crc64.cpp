#include "app.hpp"
#include "compat/crc64.hpp"
#include <cr/StringView.h>
#include <bit>

namespace floormat::Test {

namespace {

uint64_t crc64(uint64_t init, StringView str)
{
    uint64_t crc = init;
    crc = Hash::crc64_update(crc, str.data(), str.size());

    return crc;
}

} // namespace

void test_crc64()
{
    {
        constexpr uint64_t expected =  0x6C40DF5F0B497347;

        {uint64_t crc = Hash::CRC64_INITIALIZER;
         crc = crc64(crc, "123456789"_s);
         fm_assert(crc == expected);}

        {uint64_t crc = Hash::CRC64_INITIALIZER;
         crc = crc64(crc, "123"_s);
         crc = crc64(crc, "456789"_s);
         fm_assert(crc == expected);}
    }
}

} // namespace floormat::Test
