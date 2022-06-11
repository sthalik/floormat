#include "defs.hpp"
#include "anim/atlas.hpp"
#include "anim/serialize.hpp"

#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <algorithm>
#include <utility>
#include <tuple>

#include <cmath>
#include <cstring>
#include <filesystem>

using Corrade::Utility::Error;
using Corrade::Utility::Debug;

using std::filesystem::path;

struct options
{
    unsigned width = 0, height = 0;
    double scale = 0;
    path input_dir, output_dir;
};

[[nodiscard]]
static std::tuple<cv::Vec2i, cv::Vec2i, bool> find_image_bounds(const path& path, const cv::Mat4b& mat)
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

[[nodiscard]]
static bool load_file(anim_group& group, options& opts, anim_atlas& atlas, const path& filename)
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
        return false;

    cv::Size size{end - start}, dest_size;

    if (opts.scale == 0.0)
    {
        ASSERT(opts.width || opts.height);
        if (opts.width)
            opts.scale = (double)opts.width / size.width;
        else
            opts.scale = (double)opts.height / size.height;
        ASSERT(opts.scale > 1e-6);
    }

    dest_size = {(int)std::round(opts.scale * size.width),
                 (int)std::round(opts.scale * size.height)};

    if (size.width < dest_size.width || size.height < dest_size.height)
    {
        Error{} << "refusing to upscale image" << filename;
        return false;
    }

    cv::Mat4b resized{size};
    cv::resize(mat({start, size}), resized, dest_size, 0, 0, cv::INTER_LANCZOS4);
    Magnum::Vector2i ground = {
        (int)std::round((group.ground[0] - start[0]) * opts.scale),
        (int)std::round((group.ground[1] - start[1]) * opts.scale),
    };

    group.frames.push_back({ground, atlas.offset(), {dest_size.width, dest_size.height}});
    atlas.add_entry({&group.frames.back(), std::move(mat)});
    return true;
}

[[nodiscard]]
static bool load_directory(anim_group& group, options& opts, anim_atlas& atlas, const path& input_dir)
{
    if (std::error_code ec; !std::filesystem::exists(input_dir/".", ec))
    {
        Error{} << "can't open directory" << input_dir << ':' << ec.message();
        return false;
    }

    std::size_t max;
    for (max = 1; max <= 9999; max++)
    {
        char filename[9];
        sprintf(filename, "%04zu.png", max);
        if (!std::filesystem::exists(input_dir/filename))
            break;
    }
    group.frames.clear();
    // atlas stores its entries through a pointer.
    // vector::reserve() is necessary to avoid use-after-free.
    group.frames.reserve(max-1);

    for (std::size_t i = 1; i < max; i++)
    {
        char filename[9];
        sprintf(filename, "%04zu.png", i);
        if (!load_file(group, opts, atlas, input_dir/filename))
            return false;
    }

    atlas.advance_row();

    return true;
}

static char* fix_argv0(char* argv0)
{
#ifdef _WIN32
    if (auto* c = strrchr(argv0, '\\'); c && c[1])
    {
        if (auto* s = strrchr(c, '.'); s && !strcmp(".exe", s))
            *s = '\0';
        return c+1;
    }
#else
    if (auto* c = strrchr(argv[0], '/'); c && c[1])
        return c+1;
#endif
    return argv0;
}

static std::tuple<options, bool> parse_cmdline(int argc, const char* const* argv)
{
    Corrade::Utility::Arguments args{};
    args.addOption('o', "output")
        .addArgument("directory")
        .addOption('W', "width", "")
        .addOption('H', "height", "");
    args.parse(argc, argv);
    options opts;
    if (unsigned w = args.value<unsigned>("width"); w != 0)
        opts.width = w;
    if (unsigned h = args.value<unsigned>("height"); h != 0)
        opts.height = h;
    if (!(!opts.width ^ !opts.height))
    {
        Error{} << "exactly one of --width, --height must be given";
        goto usage;
    }
    opts.output_dir = args.value<std::string>("output");
    opts.input_dir = args.value<std::string>("directory");

    if (opts.output_dir.empty())
        opts.output_dir = opts.input_dir;

    return { std::move(opts), true };
usage:
    Error{Error::Flag::NoNewlineAtTheEnd} << args.usage();
    return { {}, false };
}

int main(int argc, char** argv)
{
    argv[0] = fix_argv0(argv[0]);
    auto [opts, opts_ok] = parse_cmdline(argc, argv);
    if (!opts_ok)
        return EX_USAGE;

    auto [anim_info, anim_ok] = anim::from_json(opts.input_dir/"atlas.json");

    if (!anim_ok)
        return EX_DATAERR;

    if (std::error_code error;
        !std::filesystem::exists(opts.output_dir/".") &&
        !std::filesystem::create_directory(opts.output_dir, error))
    {
        Error{} << "failed to create output directory" << opts.output_dir << ':' << error.message();
        return EX_CANTCREAT;
    }

    anim_atlas atlas;

    for (anim_group& group : anim_info.groups)
    {
        group.frames.clear(); group.frames.reserve(64);
        if (!load_directory(group, opts, atlas, opts.input_dir/group.name))
            return EX_DATAERR;
        if (!atlas.dump(opts.output_dir/"atlas.png"))
            return EX_CANTCREAT;
        if (!anim_info.to_json(opts.output_dir/"atlas.json.new"))
            return EX_CANTCREAT;
    }

    return 0;
}
