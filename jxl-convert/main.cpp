#include "jxl-encoder/encode-jxl.hpp"
#include "loader/loader.hpp"
#include "compat/fix-argv0.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cr/Path.h>
#include <cr/String.h>
#include <cr/StringView.h>
#include <cr/StridedArrayView.h>
#include <mg/Color.h>
#include <mg/ImageData.h>
#include <mg/PixelFormat.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <jxl/decode_cxx.h>
#include "compat/assert.hpp"

using namespace floormat;

namespace {

template<typename Color, typename Vec>
cv::Mat_<Vec> rows_top_first(const Trade::ImageData2D& img)
{
    const auto size = img.size();
    const auto pixels = img.pixels<Color>();
    cv::Mat_<Vec> ret(size.y(), size.x());
    for (int y = 0; y < size.y(); y++)
        for (int x = 0; x < size.x(); x++)
        {
            const auto c = pixels[usize(size.y() - 1 - y)][usize(x)];
            for (u32 i = 0; i < Color::Size; i++)
                ret(y, x)[(int)i] = c[i];
        }
    return ret;
}

bool is_decoded_exactly(ArrayView<const char> file, const cv::Mat& rgb)
{
    const auto dec = JxlDecoderMake(nullptr);
    fm_assert(dec);
    fm_assert(!JxlDecoderSubscribeEvents(&*dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE));
    fm_assert(!JxlDecoderSetInput(&*dec, reinterpret_cast<const u8*>(file.data()), file.size()));
    JxlDecoderCloseInput(&*dec);
    const JxlPixelFormat format{(u32)rgb.channels(), JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    cv::Mat out(rgb.rows, rgb.cols, rgb.type());
    u32 num_frames = 0;
    for (;;)
    {
        const auto status = JxlDecoderProcessInput(&*dec);
        if (status == JXL_DEC_BASIC_INFO)
        {
            JxlBasicInfo info;
            fm_assert(!JxlDecoderGetBasicInfo(&*dec, &info));
            fm_assert((int)info.xsize == rgb.cols && (int)info.ysize == rgb.rows);
            fm_assert(info.alpha_bits == (rgb.channels() == 4 ? 8u : 0u) && !info.have_animation);
        }
        else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER)
            fm_assert(!JxlDecoderSetImageOutBuffer(&*dec, &format, out.data, out.total() * out.elemSize()));
        else if (status == JXL_DEC_FULL_IMAGE)
            num_frames++;
        else if (status == JXL_DEC_SUCCESS)
            break;
        else
            fm_abort("decoding failed with status %d", (int)status);
    }
    return num_frames == 1 && !std::memcmp(out.data, rgb.data, out.total() * out.elemSize());
}

double seconds_since(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

int main(int argc, char** argv)
{
    argv[0] = fix_argv0(argv[0]);
    if (argc < 5)
    {
        std::fprintf(stderr, "usage: %s <threads> <effort> <out-dir> <dir>/<name>...\n", argv[0]);
        return 64;
    }
    const auto num_threads = (u32)std::atoi(argv[1]), effort = (u32)std::atoi(argv[2]);
    const StringView out_dir = argv[3];
    for (int i = 4; i < argc; i++)
    {
        const StringView arg = argv[i];
        const auto split = Path::split(arg);
        const StringView dir = split.first(), name = split.second();
        const String prefix = dir + "/"_s;
        const auto img = loader.texture(prefix, name);
        cv::Mat rgb;
        if (img.format() == PixelFormat::RGB8Unorm)
            rgb = rows_top_first<Color3ub, cv::Vec3b>(img);
        else
        {
            fm_assert(img.format() == PixelFormat::RGBA8Unorm);
            fm_assert(dir != "ground"_s && dir != "walls"_s);
            rgb = rows_top_first<Color4ub, cv::Vec4b>(img);
        }
        cv::Mat bgr;
        cv::cvtColor(rgb, bgr, rgb.channels() == 4 ? cv::COLOR_RGBA2BGRA : cv::COLOR_RGB2BGR);
        const auto t0 = std::chrono::steady_clock::now();
        const auto file = encode_jxl(bgr, num_threads, effort);
        fm_assert(file);
        const double s = seconds_since(t0);
        const bool exact = is_decoded_exactly(*file, rgb);
        fm_assert(Path::make(Path::join(out_dir, dir)));
        fm_assert(Path::write(Path::join(out_dir, arg + ".jxl"_s), *file));
        std::printf("%s %dx%d %d channels, %zu bytes, %.1f s, %s\n", String{arg}.data(), rgb.cols, rgb.rows,
                    rgb.channels(), file->size(), s, exact ? "exact" : "NOT EXACT");
        std::fflush(stdout);
        if (!exact)
            return 1;
    }
    return 0;
}
