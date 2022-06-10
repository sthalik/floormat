#include "../defs.hpp"
#include "serialize.hpp"
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <optional>
#include <tuple>
#include <filesystem>
#include <algorithm>
#include <utility>
#include <cstring>
#include <cmath>

#undef MIN
#undef MAX

#ifdef _WIN32
#   define EX_OK        0   /* successful termination */
#   define EX_USAGE     64  /* command line usage error */
#   define EX_DATAERR   65  /* data format error */
#   define EX_SOFTWARE  70  /* internal software error */
#   define EX_CANTCREAT 73  /* can't create (user) output file */
#   define EX_IOERR     74  /* input/output error */
#else
#   include <sysexits.h>
#endif

using Corrade::Utility::Error;
using Corrade::Utility::Debug;

static struct options_ {
    std::optional<unsigned> width, height;
    std::optional<double> scale;
} options;

using std::filesystem::path;

static
std::tuple<cv::Vec2i, cv::Vec2i, bool>
find_image_bounds(const path& path, const cv::Mat4b& mat)
{
    cv::Vec2i start{mat.cols, mat.rows}, end{0, 0};
    for (int y = 0; y < mat.rows; y++)
    {
        const auto* ptr = mat.ptr<cv::Vec4b>(y);
        for (int x = 0; x < mat.cols; x++)
        {
            enum {R, G, B, A};
            cv::Vec4b px = ptr[x];
            if (px[A] != 0)
            {
                start[0] = std::min(x, start[0]);
                start[1] = std::min(y, start[1]);
                end[0] = std::max(x+1, end[0]);
                end[1] = std::max(y+1, end[1]);
            }
        }
    }
    if (start[0] >= end[0] || start[1] >= end[1])
    {
        Error{} << "image" << path << "contains only fully transparent pixels!";
        return {{}, {}, false};
    }

    return {start, end, true};
}

static bool load_file(anim_group& group, const path& filename, const path& output_filename)
{
    auto mat = progn(
        cv::Mat mat_ = cv::imread(filename.string(), cv::IMREAD_UNCHANGED);
        if (mat_.empty() || mat_.type() != CV_8UC4)
        {
            Error{} << "failed to load" << filename << "as RGBA32 image";
            return cv::Mat4b{};
        }
        return cv::Mat4b(std::move(mat_));
    );

    if (mat.empty())
        return false;

    auto [start, end, bounds_ok] = find_image_bounds(filename, mat);

    if (!bounds_ok)
        return {};

    cv::Size size{end - start}, dest_size;

    if (!options.scale)
    {
        if (options.width)
            options.scale = (double)*options.width / size.width;
        else if (options.height)
            options.scale = (double)*options.height / size.height;
        else
            std::abort();
    }

    dest_size = {(int)std::round(*options.scale * size.width),
                 (int)std::round(*options.scale * size.height)};

    if (size.width < dest_size.width || size.height < dest_size.height)
    {
        Error{} << "refusing to upscale image" << filename;
        return {};
    }

    cv::Mat4b resized{size};
    cv::resize(mat({start, size}), resized, dest_size, 0, 0, cv::INTER_LANCZOS4);
    if (!cv::imwrite(output_filename.string(), resized))
    {
        Error{} << "failed writing image" << output_filename;
        return false;
    }
    Magnum::Vector2i ground = {
        (int)std::round((group.ground[0] - start[0]) * *options.scale),
        (int)std::round((group.ground[1] - start[1]) * *options.scale),
    };
    group.frames.push_back({ground});
    return true;
}

static bool load_directory(anim_group& group, const path& input_dir, const path& output_dir)
{
    if (std::error_code ec{}; !std::filesystem::exists(input_dir/".", ec))
    {
        Error{} << "can't open directory" << input_dir << ':' << ec.message();
        return {};
    }

    int i;
    for (i = 1; i <= 9999; i++)
    {
        char filename[9];
        sprintf(filename, "%04d.png", i);
        if (!std::filesystem::exists(input_dir/filename))
            break;
        if (!load_file(group, input_dir/filename, output_dir/filename))
            return false;
    }

    if (i == 1)
    {
        Error{} << "no files in anim group directory" << input_dir;
        return false;
    }

    return true;
}

int main(int argc, char** argv)
{
    Corrade::Utility::Arguments args{};
#ifdef _WIN32
    if (auto* c = strrchr(argv[0], '\\'); c && c[1])
    {
        if (auto* s = strrchr(c, '.'); s && !strcmp(".exe", s))
            *s = '\0';
        args.setCommand(c+1);
    }
#else
    if (auto* c = strrchr(argv[0], '/'); c && c[1])
        args.setCommand(c+1);
#endif
    args.addOption('o', "output", "./output")
        .addArgument("directory")
        .addOption('W', "width", "")
        .addOption('H', "height", "");
    args.parse(argc, argv);
    const path output_dir = args.value<std::string>("output");
    const path input_dir = args.value<std::string>("directory");
    auto anim_info = anim::from_json(input_dir / "atlas.json");

    if (!anim_info)
        goto usage;

    if (unsigned w = args.value<unsigned>("width"); w != 0)
        options.width = w;
    if (unsigned h = args.value<unsigned>("height"); h != 0)
        options.height = h;
    if (!(!options.width ^ !options.height))
    {
        Error{} << "exactly one of --width, --height must be given";
        goto usage;
    }

    try {
        std::filesystem::create_directory(output_dir);
    } catch (const std::filesystem::filesystem_error& error) {
        Error{} << "failed to create output directory" << output_dir << ':' << error.what();
        return EX_CANTCREAT;
    }

    for (std::size_t i = 0; i < (std::size_t)anim_direction::COUNT; i++)
    {
        auto group_name = anim_group::direction_to_string((anim_direction)i);
        try {
            std::filesystem::remove_all(output_dir/group_name);
            std::filesystem::create_directory(output_dir/group_name);
        } catch (const std::filesystem::filesystem_error& e) {
            Error{} << "failed creating output directory" << group_name << ':' << e.what();
            return EX_CANTCREAT;
        }
        auto& group = anim_info->groups[i];
        group.frames.clear(); group.frames.reserve(64);
        if (!load_directory(group, input_dir/group_name, output_dir/group_name))
            return EX_DATAERR;
        if (!anim_info->to_json(output_dir/"atlas.json"))
            return EX_CANTCREAT;
    }

    return 0;

usage:
    Error{Error::Flag::NoNewlineAtTheEnd} << Corrade::Containers::StringView{args.usage()};
    return EX_USAGE;
}
