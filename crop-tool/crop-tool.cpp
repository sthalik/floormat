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
#   define EX_SOFTWARE  70  /* internal software error */
#   define EX_IOERR     74  /* input/output error */
#else
#   include <sysexits.h>
#endif

#if 0
static std::string fix_path_separators(const std::filesystem::path& path)
{
    auto str = path.string();
    std::replace(str.begin(), str.end(), '\\', '/');
    return str;
}
#endif

struct file
{
    std::filesystem::path name;
    cv::Mat4b mat;
    cv::Point2i offset;
};

struct dir
{
    std::filesystem::path name;
    std::vector<file> files;
};

using Corrade::Utility::Error;
using Corrade::Utility::Debug;

static struct options_ {
    std::optional<unsigned> width, height;
    std::optional<double> scale;
    cv::Vec2i offset{-1, -1};
} options;

static
std::tuple<cv::Vec2i, cv::Vec2i, bool>
find_image_bounds(const std::filesystem::path& path, const cv::Mat4b& mat)
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
        Error{} << "image" << path.string() << "contains only fully transparent pixels!";
        return {{}, {}, false};
    }

    return {start, end, true};
}

static std::optional<file> load_file(const std::filesystem::path& filename)
{
    auto mat = progn(
        cv::Mat mat_ = cv::imread(filename.string(), cv::IMREAD_UNCHANGED);
        if (mat_.empty() || mat_.type() != CV_8UC4)
        {
            Error{} << "failed to load" << filename.string() << "as RGBA32 image";
            return cv::Mat4b{};
        }
        return cv::Mat4b(std::move(mat_));
    );

    if (mat.empty())
        return {};

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

    cv::Vec2i offset = {
            (int)std::round((options.offset[0] - start[0]) * *options.scale),
            (int)std::round((options.offset[1] - start[1]) * *options.scale),
    };

    dest_size = {(int)std::round(*options.scale * size.width),
                 (int)std::round(*options.scale * size.height)};

    if (size.width < dest_size.width || size.height < dest_size.height)
    {
        Error{} << "refusing to upscale image" << filename.string();
        return {};
    }

    Debug{} << "file" << filename.string() << offset[0] << offset[1];

    cv::Mat4b resized{size};
    cv::resize(mat({start, size}), resized, dest_size, 0, 0, cv::INTER_LANCZOS4);
    file ret { filename, resized.clone(), offset };
    return std::make_optional(std::move(ret));
}

static std::optional<dir> load_directory(const std::filesystem::path& dirname)
{
    if (std::error_code ec{}; !std::filesystem::exists(dirname / ".", ec))
    {
        Error{} << "can't open directory" << dirname.string() << ":" << ec.message();
        return std::nullopt;
    }

    dir ret;
    for (int i = 1; i <= 9999; i++)
    {
        char buf[9];
        sprintf(buf, "%04d.png", i);
        auto path = dirname / buf;
        if (!std::filesystem::exists(path))
            break;
        auto file = load_file(path);
        if (!file)
            return std::nullopt;
        ret.files.push_back(std::move(*file));
    }
    if (ret.files.empty())
    {
        Error{} << "directory" << dirname.string() << "is empty!";
        return std::nullopt;
    }
    return std::make_optional(std::move(ret));
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
        .addOption('H', "height", "")
        .addOption('x', "offset")
        .setHelp("offset", {}, "WxH");
    args.parse(argc, argv);
    std::filesystem::path output = args.value<std::string>("output");
    std::filesystem::path pathname = args.value<std::string>("directory");
    std::vector<dir> dirs;
    auto anim_info = anim::from_json(pathname / "atlas.json");

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

    {
        auto str = args.value<std::string>("offset");
        if (str.empty())
        {
            Error{} << "offset argument is required";
            goto usage;
        }
        int x, y;
        int ret = std::sscanf(str.c_str(), "%dx%d", &x, &y);
        if (ret != 2)
        {
            Error{} << "can't parse offset --" << str;
            goto usage;
        }
        options.offset = {x, y};
    }

    using anim_dir_t = std::underlying_type_t<anim_direction>;
    for (auto i = (anim_dir_t)anim_direction::MIN; i < (anim_dir_t)anim_direction::MAX; i++)
    {
        auto name = anim_direction_group::anim_direction_string((anim_direction)i);
        auto result = load_directory(pathname / name);
        if (!result)
            goto usage;
        dirs.push_back(std::move(*result));
    }

    return 0;
usage:
    Error{Error::Flag::NoNewlineAtTheEnd} << Corrade::Containers::StringView{args.usage()};
    return EX_USAGE;
}
