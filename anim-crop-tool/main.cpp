#include "atlas.hpp"
#include "serialize/anim.hpp"
#include "serialize/magnum-vector2i.hpp"
#include "serialize/json-helper.hpp"
#include "compat/defs.hpp"
#include "compat/sysexits.hpp"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <utility>
#include <tuple>
#include <string_view>
#include <filesystem>

#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/String.h>

#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Path.h>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "compat/assert.hpp"

using namespace floormat;
using namespace floormat::Serialize;

using Corrade::Utility::Error;
using Corrade::Utility::Debug;

struct options
{
    double scale = 0;
    String input_dir, input_file, output_dir;
    std::size_t width = 0, height = 0, nframes = 0;
};

[[nodiscard]]
static std::tuple<cv::Vec2i, cv::Vec2i, bool> find_image_bounds(const cv::Mat4b& mat) noexcept
{
    cv::Vec2i start{mat.cols, mat.rows}, end{0, 0};
    for (int y = 0; y < mat.rows; y++)
    {
        const auto* ptr = mat.ptr<cv::Vec4b>(y);
        for (int x = 0; x < mat.cols; x++)
        {
            enum {R, G, B, A};
            if (cv::Vec4b px = ptr[x]; px[A] != 0)
            {
                start[0] = std::min(x, start[0]);
                start[1] = std::min(y, start[1]);
                end[0] = std::max(x+1, end[0]);
                end[1] = std::max(y+1, end[1]);
            }
        }
    }
    if (start[0] < end[0] && start[1] < end[1])
        return {start, end, true};
    else
        return {{}, {}, false};
}

[[nodiscard]]
static bool load_file(anim_group& group, options& opts, anim_atlas& atlas, StringView filename)
{
    auto mat = fm_begin(
        cv::Mat mat = cv::imread(filename, cv::IMREAD_UNCHANGED);
        if (mat.empty() || mat.type() != CV_8UC4)
        {
            Error{} << "error: failed to load" << filename << "as RGBA32 image";
            return cv::Mat4b{};
        }
        return cv::Mat4b(std::move(mat));
    );

    if (mat.empty())
        return false;

    auto [start, end, bounds_ok] = find_image_bounds(mat);

    if (!bounds_ok)
    {
        Error{} << "error: no valid image data in" << filename;
        return false;
    }

    cv::Size size{end - start};

    if (opts.scale == 0.0)
    {
        fm_assert(opts.width || opts.height);
        if (opts.width)
            opts.scale = (double)opts.width / size.width;
        else
            opts.scale = (double)opts.height / size.height;
        fm_assert(opts.scale > 1e-6);
    }

    const cv::Size dest_size = {
        (int)std::round(opts.scale * size.width),
        (int)std::round(opts.scale * size.height)
    };

    if (size.width < dest_size.width || size.height < dest_size.height)
    {
        Error{} << "error: refusing to upscale image" << filename;
        return false;
    }

    cv::Mat4b resized{size};
    cv::resize(mat({start, size}), resized, dest_size, 0, 0, cv::INTER_LANCZOS4);

    const Vector2i ground = {
        (int)std::round(((int)group.ground[0] - start[0]) * opts.scale),
        (int)std::round(((int)group.ground[1] - start[1]) * opts.scale),
    };

    const Vector2ui dest_size_ = { (unsigned)dest_size.width, (unsigned)dest_size.height };

    group.frames.push_back({ground, atlas.offset(), dest_size_});
    atlas.add_entry({&group.frames.back(), std::move(resized)});
    return true;
}

[[nodiscard]]
static bool load_directory(anim_group& group, options& opts, anim_atlas& atlas)
{
    const auto input_dir = Path::join(opts.input_dir, group.name);

    if (!Path::exists(Path::join(input_dir, ".")))
    {
        Error{} << "error: can't open directory" << input_dir;
        return false;
    }

    unsigned max;
    for (max = 1; max <= 9999; max++)
    {
        char filename[9];
        sprintf(filename, "%04d.png", max);
        if (!Path::exists(Path::join(input_dir, filename)))
            break;
    }

    if (max == 1)
    {
        Error{Error::Flag::NoSpace} << "no files in directory " << input_dir << "!";
        return false;
    }

    if (!opts.nframes)
        opts.nframes = max-1;
    else if (opts.nframes != max-1)
    {
        Error{} << "error: wrong frame count for direction" << group.name << ":"
                << max-1 << "should be" << opts.nframes;
        return false;
    }

    group.frames.clear();
    // atlas stores its entries through a pointer.
    // vector::reserve() is necessary to avoid use-after-free.
    group.frames.reserve((std::size_t)max-1);

    for (unsigned i = 1; i < max; i++)
    {
        char filename[9];
        sprintf(filename, "%04d.png", i);
        if (!load_file(group, opts, atlas, Path::join(input_dir, filename)))
            return false;
    }

    atlas.advance_row();

    return true;
}

static char* fix_argv0(char* argv0) noexcept
{
#ifdef _WIN32
    if (auto* c = strrchr(argv0, '\\'); c && c[1])
    {
        if (auto* s = strrchr(c, '.'); s && !strcmp(".exe", s))
            *s = '\0';
        return c+1;
    }
#else
    if (auto* c = strrchr(argv0, '/'); c && c[1])
        return c+1;
#endif
    return argv0;
}

using Corrade::Utility::Arguments;

static std::tuple<options, Arguments, bool> parse_cmdline(int argc, const char* const* argv) noexcept
{
    Corrade::Utility::Arguments args{};
    args.addOption('o', "output")
        .addArgument("input")
        .addOption('W', "width", "")
        .addOption('H', "height", "");
    args.parse(argc, argv);
    options opts;
    if (auto w = args.value<unsigned>("width"); w != 0)
        opts.width = w;
    if (auto h = args.value<unsigned>("height"); h != 0)
        opts.height = h;
    opts.output_dir = args.value<std::string>("output");
    opts.input_file = args.value<std::string>("input");
    opts.input_dir = Path::split(opts.input_file).first();

    if (opts.output_dir.isEmpty())
        opts.output_dir = opts.input_dir;

    return { std::move(opts), std::move(args), true };
}

[[nodiscard]] static int usage(const Arguments& args) noexcept
{
    Error{Error::Flag::NoNewlineAtTheEnd} << args.usage();
    return EX_USAGE;
}

[[nodiscard]] static bool check_atlas_name(const std::string& str) noexcept
{
    constexpr auto npos = std::string::npos;

    if (str.empty())
        return false;
    if (str[0] == '.' || str[0] == '\\' || str[0] == '/')
        return false;
    if (str.find('"') != npos || str.find('\'') != npos)
        return false;
    if (str.find("/.") != npos || str.find("\\.") != npos)
        return false; // NOLINT(readability-simplify-boolean-expr)

    return true;
}

int main(int argc, char** argv)
{
    argv[0] = fix_argv0(argv[0]);
    auto [opts, args, opts_ok] = parse_cmdline(argc, argv);
    if (!opts_ok)
        return usage(args);

    auto anim_info = floormat::json_helper::from_json<anim>(opts.input_file);

    if (!check_atlas_name(anim_info.object_name))
    {
        Error{} << "error: atlas object name" << anim_info.object_name << "is invalid";
        return EX_DATAERR;
    }

    if (!check_atlas_name(anim_info.anim_name))
    {
        Error{} << "error: atlas animation name" << anim_info.object_name << "contains invalid characters";
        return EX_DATAERR;
    }

    if (!opts.width)
        opts.width = anim_info.width;
    if (!opts.height)
        opts.height = anim_info.height;
    opts.nframes = anim_info.nframes;

    if (!(opts.width ^ opts.height))
    {
        Error{} << "error: exactly one of --width, --height must be specified";
        return usage(args);
    }

    anim_atlas atlas;

    for (anim_group& group : anim_info.groups)
        if (!load_directory(group, opts, atlas))
            return EX_DATAERR;

    if (std::error_code ec{}; !Path::exists(Path::join(opts.output_dir, ".")) &&
                              !std::filesystem::create_directory(opts.output_dir.data(), ec)) {
        Error{} << "error: failed to create output directory"
                << opts.output_dir << ":" << ec.message();
        return EX_CANTCREAT;
    }

    const String base_name = anim_info.object_name + "_" + anim_info.anim_name;

    if (auto pathname = Path::join(opts.output_dir, (base_name + ".png")); !atlas.dump(pathname)) {
        Error{} << "error: failed writing image to" << pathname << ":"
                << std::strerror(errno); // NOLINT(concurrency-mt-unsafe)
        return EX_CANTCREAT;
    }
    anim_info.pixel_size = Vector2ui(atlas.size());
    floormat::json_helper::to_json<anim>(anim_info, Path::join(opts.output_dir, (base_name + ".json")));

    return 0;
}
