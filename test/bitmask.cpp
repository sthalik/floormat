#include "app.hpp"
#include "compat/assert.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat {

namespace {

const unsigned char img_bitmask[] = {
#include "bitmask.embed.inc"
};

constexpr auto data_nbytes = arraySize(img_bitmask);
constexpr auto size = Vector2i{21, 52};
static_assert(size_t{size.product()+7}/8 == data_nbytes);

void bitmask_test()
{
    auto img = loader.texture(loader.SCENERY_PATH, "control-panel"_s);
    auto bitmask = anim_atlas::make_bitmask(img);
    fm_assert(bitmask.size() == size_t{size.product()});
    fm_assert(img.pixelSize() == 4 && img.size() == size);
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
    fm_assert(img.size().product() == Int{data_nbytes});
    const auto len = std::min(data_nbytes, (size_t)bitmask.size()+7 >> 3);
    for (auto i = 0uz; i < len; i++)
        if ((unsigned char)bitmask.data()[i] != img_bitmask[i])
            fm_abort("wrong value at bit %zu, should be' 0x%02hhx'", i, img_bitmask[i]);
}

} // namespace

void test_app::test_bitmask()
{
    bitmask_test();
    //bitmask_benchmark();
}

} // namespace floormat
