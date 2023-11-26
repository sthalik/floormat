#include "atlas.hpp"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include "compat/debug.hpp"
#include "compat/sysexits.hpp"
#include "compat/fix-argv0.hpp"
#include "loader/loader.hpp"
#include "serialize/magnum-vector.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <tuple>

#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/String.h>

#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Move.h>
#include <Corrade/Utility/Path.h>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

using namespace floormat;

using Corrade::Utility::Error;
using Corrade::Utility::Debug;
using floormat::Serialize::anim_atlas_;

namespace {

struct options
{
    String input_dir, input_file, output_dir;
    size_t nframes = 0;
    anim_scale scale;
};

[[nodiscard]]
std::tuple<cv::Vec2i, cv::Vec2i, bool> find_image_bounds(const cv::Mat4b& mat) noexcept
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
bool load_file(anim_group& group, options& opts, anim_atlas_& atlas, StringView filename)
{
    auto mat = fm_begin(
        cv::Mat mat = cv::imread(filename, cv::IMREAD_UNCHANGED);
        if (mat.empty() || mat.type() != CV_8UC4)
        {
            Error{} << "error: failed to load" << filename << "as RGBA32 image";
            return cv::Mat4b{};
        }
        return cv::Mat4b(Utility::move(mat));
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
    if (opts.scale.type != anim_scale_type::ratio)
    {
        float new_width = opts.scale.scale_to_({(unsigned)size.width, (unsigned)size.height})[0];
        opts.scale = anim_scale::ratio{new_width / (float)size.width};
    }

    cv::Size dest_size;
    {
        auto xy = opts.scale.scale_to({(unsigned)size.width, (unsigned)size.height});
        dest_size = cv::Size{(int)xy[0], (int)xy[1]};
    }

    const auto factor = (float)dest_size.width / (float)size.width;

    if (size.width < dest_size.width || size.height < dest_size.height)
    {
        Error{} << "error: refusing to upscale image" << filename;
        return false;
    }

    cv::Mat4b resized{size};
    cv::resize(mat({start, size}), resized, dest_size, 0, 0, cv::INTER_LANCZOS4);

    const Vector2i ground = {
        (int)std::round(((int)group.ground[0] - start[0]) * factor),
        (int)std::round(((int)group.ground[1] - start[1]) * factor),
    };

    const Vector2ui dest_size_ = { (unsigned)dest_size.width, (unsigned)dest_size.height };

    group.frames.push_back({ground, atlas.offset(), dest_size_});
    atlas.add_entry({&group.frames.back(), Utility::move(resized)});
    return true;
}

[[nodiscard]]
bool load_directory(anim_group& group, options& opts, anim_atlas_& atlas)
{
    if (!group.mirror_from.isEmpty())
        return true;

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
    group.frames.reserve((size_t)max-1);

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

inline String fixsep(String str)
{
#ifdef _WIN32
    for (char& c : str)
        if (c == '\\')
            c = '/';
#endif
    return str;
}

using Corrade::Utility::Arguments;

std::tuple<options, Arguments, bool> parse_cmdline(int argc, const char* const* argv) noexcept
{
    Corrade::Utility::Arguments args{};
    args.addOption('o', "output").setHelp("output", "", "DIR")
        .addArgument("input.json")
        .addOption('W', "width", "")
        .addOption('H', "height", "")
        .addOption('F', "scale", "");

    args.parse(argc, argv);
    options opts;

    if (!args.value<StringView>("width").isEmpty())
        opts.scale = anim_scale::fixed{args.value<unsigned>("width"), true};
    else if (!args.value<StringView>("height").isEmpty())
        opts.scale = anim_scale::fixed{args.value<unsigned>("height"), false};
    else if (!args.value<StringView>("scale").isEmpty())
        opts.scale = anim_scale::ratio{args.value<float>("scale")};

    opts.output_dir = Path::join(loader.startup_directory(), fixsep(args.value<StringView>("output")));
    opts.input_file = Path::join(loader.startup_directory(), fixsep(args.value<StringView>("input.json")));
    opts.input_dir = Path::split(opts.input_file).first();

    if (opts.output_dir.isEmpty())
        opts.output_dir = opts.input_dir;

    return { Utility::move(opts), Utility::move(args), true };
}

[[nodiscard]] int usage(const Arguments& args) noexcept
{
    Error{Error::Flag::NoNewlineAtTheEnd} << args.usage();
    return EX_USAGE;
}

} // namespace

int main(int argc, char** argv)
{
    argv[0] = fix_argv0(argv[0]);
    auto [opts, args, opts_ok] = parse_cmdline(argc, argv);
    if (!opts_ok)
        return usage(args);

    auto anim_info = json_helper::from_json<anim_def>(opts.input_file);

    if (!loader.check_atlas_name(anim_info.object_name))
    {
        Error{} << "error: atlas object name" << anim_info.object_name << "is invalid";
        return EX_DATAERR;
    }

    if (!anim_info.anim_name.isEmpty() && !loader.check_atlas_name(anim_info.anim_name))
    {
        Error{} << "error: atlas animation name" << anim_info.object_name << "is invalid";
        return EX_DATAERR;
    }

    opts.nframes = anim_info.nframes;
    if (opts.scale.type == anim_scale_type::invalid)
        opts.scale = anim_info.scale;
    anim_atlas_ atlas;

    for (anim_group& group : anim_info.groups)
        if (!load_directory(group, opts, atlas))
            return EX_DATAERR;

    if (!Path::make(opts.output_dir))
        return EX_CANTCREAT;

    const String base_name = !anim_info.anim_name.isEmpty()
                             ? anim_info.object_name + "-" + anim_info.anim_name
                             : anim_info.object_name;

    if (auto pathname = Path::join(opts.output_dir, (base_name + ".png")); !atlas.dump(pathname)) {
        auto errstr = error_string();
        ERR << "error: failed writing image to" << quoted(pathname) << colon() << errstr;
        return EX_CANTCREAT;
    }
    anim_info.pixel_size = Vector2ui(atlas.size());
    json_helper::to_json<anim_def>(anim_info, Path::join(opts.output_dir, (base_name + ".json")));

    return 0;
}
