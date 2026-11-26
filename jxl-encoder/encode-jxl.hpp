#pragma once
#include <cr/Array.h>
#include <cr/Optional.h>

namespace cv { class Mat; }

namespace floormat {

[[nodiscard]] Optional<Array<char>> encode_jxl(const cv::Mat& image, u32 num_threads, u32 effort);

} // namespace floormat
