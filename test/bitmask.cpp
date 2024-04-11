#include "app.hpp"
#include "compat/assert.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <mg/Functions.h>
#include <mg/ImageData.h>

namespace floormat {

namespace {

const unsigned char src[] = {
#include "bitmask.embed.inc"
};

constexpr auto data_nbytes = arraySize(src);
constexpr auto size = Vector2i{21, 52};
//static_assert(size_t{size.product()+7}/8 <= data_nbytes);

void bitmask_test()
{
    auto img = loader.texture("images/", "bitmask-test1"_s);
    auto bitmask = anim_atlas::make_bitmask(img);
    fm_assert(bitmask.size() >= size_t{size.product()});
    fm_assert(img.pixelSize() == 4);
//#define DO_GENERATE
#ifdef DO_GENERATE
    fputc('\n', stdout);
    for (auto i = 0u; i < (bitmask.size()+7)/8; i++)
    {
        printf("0x%02hhx,", bitmask.data()[i]);
        if (i % 15 == 14)
            printf("\n");
    }
    printf("\n");
    fflush(stdout);
#endif
    const auto len = Math::min(data_nbytes, (size_t)bitmask.size()+7 >> 3);
    fm_assert(arraySize(src) >= len);
    for (auto i = 0uz; i < len; i++)
    {
        auto a = (unsigned char)bitmask.data()[i];
        if (a != src[i])
            fm_abort("wrong value 0x%02hhx at byte %zu, should be' 0x%02hhx'", a, i, src[i]);
    }
}

} // namespace

void test_app::test_bitmask()
{
    bitmask_test();
    //bitmask_benchmark();
}

} // namespace floormat
