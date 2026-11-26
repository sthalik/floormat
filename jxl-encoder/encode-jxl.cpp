#include "encode-jxl.hpp"
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <jxl/encode_cxx.h>
#include <jxl/thread_parallel_runner_cxx.h>
#include "compat/assert.hpp"

namespace floormat {

namespace {

const char* encoder_error_string(JxlEncoderError error)
{
    switch (error)
    {
    case JXL_ENC_ERR_OK: return "no error";
    case JXL_ENC_ERR_GENERIC: return "generic error";
    case JXL_ENC_ERR_OOM: return "out of memory";
    case JXL_ENC_ERR_JBRD: return "JPEG reconstruction data error";
    case JXL_ENC_ERR_BAD_INPUT: return "bad input";
    case JXL_ENC_ERR_NOT_SUPPORTED: return "not supported";
    case JXL_ENC_ERR_API_USAGE: return "API usage error";
    }
    return "unknown error";
}

} // namespace

Optional<Array<char>> encode_jxl(const cv::Mat& image, u32 num_threads, u32 effort)
{
    fm_assert(image.type() == CV_8UC3 || image.type() == CV_8UC4);
    fm_assert(effort >= 1 && effort <= 11);
    fm_assert(image.cols > 0 && image.rows > 0);

    const u32 channel_count = (u32)image.channels();
    const bool has_alpha = channel_count == 4;

    cv::Mat pixels;
    cv::cvtColor(image, pixels, has_alpha ? cv::COLOR_BGRA2RGBA : cv::COLOR_BGR2RGB);
    fm_assert(pixels.isContinuous());

    const JxlEncoderPtr encoder = JxlEncoderMake(nullptr);
    if (!encoder)
    {
        ERR << "encode_jxl(): can't create an encoder";
        return {};
    }

    const usize num_worker_threads = num_threads ? num_threads : JxlThreadParallelRunnerDefaultNumWorkerThreads();
    JxlThreadParallelRunnerPtr runner;
    if (num_worker_threads > 1)
    {
        runner = JxlThreadParallelRunnerMake(nullptr, num_worker_threads);
        if (!runner || JxlEncoderSetParallelRunner(encoder.get(), JxlThreadParallelRunner, runner.get()) != JXL_ENC_SUCCESS)
        {
            ERR << "encode_jxl(): can't start" << num_worker_threads << "threads";
            return {};
        }
    }

    JxlEncoderAllowExpertOptions(encoder.get());

    JxlBasicInfo info;
    JxlEncoderInitBasicInfo(&info);
    info.xsize = (u32)pixels.cols;
    info.ysize = (u32)pixels.rows;
    info.bits_per_sample = 8;
    info.num_color_channels = 3;
    info.num_extra_channels = has_alpha ? 1 : 0;
    info.alpha_bits = has_alpha ? 8 : 0;
    info.uses_original_profile = JXL_TRUE;
    if (JxlEncoderSetBasicInfo(encoder.get(), &info) != JXL_ENC_SUCCESS)
    {
        ERR << "encode_jxl(): can't set basic info:" << encoder_error_string(JxlEncoderGetError(encoder.get()));
        return {};
    }

    JxlColorEncoding color_encoding;
    JxlColorEncodingSetToSRGB(&color_encoding, JXL_FALSE);
    if (JxlEncoderSetColorEncoding(encoder.get(), &color_encoding) != JXL_ENC_SUCCESS)
    {
        ERR << "encode_jxl(): can't set color encoding:" << encoder_error_string(JxlEncoderGetError(encoder.get()));
        return {};
    }

    JxlEncoderFrameSettings* const frame_settings = JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
    if (!frame_settings ||
        JxlEncoderSetFrameLossless(frame_settings, JXL_TRUE) != JXL_ENC_SUCCESS ||
        JxlEncoderFrameSettingsSetOption(frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, (i64)effort) != JXL_ENC_SUCCESS)
    {
        ERR << "encode_jxl(): can't set frame settings:" << encoder_error_string(JxlEncoderGetError(encoder.get()));
        return {};
    }

    const JxlPixelFormat pixel_format{channel_count, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    const usize image_size = pixels.total() * pixels.elemSize();
    if (JxlEncoderAddImageFrame(frame_settings, &pixel_format, pixels.data, image_size) != JXL_ENC_SUCCESS)
    {
        ERR << "encode_jxl(): can't add the image:" << encoder_error_string(JxlEncoderGetError(encoder.get()));
        return {};
    }
    JxlEncoderCloseInput(encoder.get());

    Array<char> output;
    arrayResize(output, NoInit, Math::max(image_size/2, usize{4096}));
    usize output_size = 0;
    for (;;)
    {
        auto* next = reinterpret_cast<u8*>(output.data() + output_size);
        usize available = output.size() - output_size;
        const JxlEncoderStatus status = JxlEncoderProcessOutput(encoder.get(), &next, &available);
        output_size = usize(reinterpret_cast<char*>(next) - output.data());
        if (status == JXL_ENC_SUCCESS)
            break;
        else if (status == JXL_ENC_NEED_MORE_OUTPUT)
            arrayResize(output, NoInit, output.size()*2);
        else
        {
            ERR << "encode_jxl(): encoding failed:" << encoder_error_string(JxlEncoderGetError(encoder.get()));
            return {};
        }
    }
    arrayResize(output, output_size);
    arrayShrink(output);
    return output;
}

} // namespace floormat
