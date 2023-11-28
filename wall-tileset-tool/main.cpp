#include "main.hpp"
#include "compat/assert.hpp"
#include "compat/sysexits.hpp"
#include "compat/fix-argv0.hpp"
#include "compat/format.hpp"
#include "compat/debug.hpp"
#include "src/wall-atlas.hpp"
//#include "serialize/wall-atlas.hpp"
//#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
#include <utility>
#include <tuple>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Containers/TripleStl.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Arguments.h>
#include <Magnum/Math/Functions.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

namespace floormat::wall_tool {

using Corrade::Utility::Arguments;
using namespace std::string_literals;
using namespace floormat::Wall;

namespace {

const Direction& get_direction(const wall_atlas_def& atlas, size_t i)
{
    fm_assert(atlas.direction_mask[i]);
    auto idx = atlas.direction_map[i];
    fm_assert(idx);
    fm_assert(idx.val < atlas.direction_array.size());
    return atlas.direction_array[idx.val];
}

Direction& get_direction(wall_atlas_def& atlas, size_t i)
{
    return const_cast<Direction&>(get_direction(const_cast<const wall_atlas_def&>(atlas), i));
}

template<typename Fmt, typename... Xs>
auto asformat(Fmt&& fmt, Xs&&... args)
{
    auto result = fmt::format_to_n((char*)nullptr, 0, std::forward<Fmt>(fmt), std::forward<Xs>(args)...);
    std::string ret;
    ret.resize(result.size);
    auto result2 = fmt::format_to_n(ret.data(), ret.size(), std::forward<Fmt>(fmt), std::forward<Xs>(args)...);
    fm_assert(result2.size == result.size);
    return ret;
}

struct resolution : Vector2i { using Vector2i::Vector2i; };

Debug& operator<<(Debug& dbg, resolution res)
{
    auto flags = dbg.flags();
    dbg << "";
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << res.x() << "x"_s << res.y();
    dbg.setFlags(flags);
    return dbg;
}

constexpr inline int max_image_dimension = 4096;

bool convert_to_bgra32(const cv::Mat& src, cv::Mat4b& dest)
{
    fm_assert(dest.empty() || dest.size == src.size);
    auto ch = src.channels(), tp = src.type();

    switch (auto type = src.type())
    {
    default:
        return false;
    case CV_8U:
        cv::cvtColor(src, dest, cv::COLOR_GRAY2BGRA);
        return true;
    case CV_8UC3:
        cv::cvtColor(src, dest, cv::COLOR_BGR2BGRA);
        return true;
    case CV_8UC4:
        src.copyTo(dest);
        return true;
    }
}

bool do_group(state st, size_t i, size_t j, Group& new_group)
{
    const auto group_name = Direction::groups[j].name;
    const wall_atlas_def& old_atlas = st.old_atlas;
    wall_atlas_def& new_atlas = st.new_atlas;
    const auto& old_dir = get_direction(old_atlas, (size_t)i);
    const auto& old_group = old_dir.group(j);
    //auto& new_dir = get_direction(new_atlas, (size_t)i);
    const auto dir_name = wall_atlas::directions[i].name;

    const auto path = Path::join({ st.opts.input_dir, dir_name, group_name });
    const auto expected_size = wall_atlas::expected_size(new_atlas.header.depth, (Group_)j);

    DBG << "    group" << quoted2(group_name);
    fm_debug_assert(expected_size > Vector2ui{0});
    fm_assert(Math::max(expected_size.x(), expected_size.y()) < max_image_dimension);

    uint32_t count = 0, start = (uint32_t)st.frames.size();
    new_group = old_group;
    new_group.is_defined = true;
    new_group.pixel_size = Vector2ui(expected_size);

    if (old_group.from_rotation == (uint8_t)-1)
    {
        for (;;)
        {
            auto filename = asformat("{}/{:04}.png"_cf, path, count+1);
            if (!Path::exists(filename))
                break;
            count++;
            if (Path::isDirectory(filename)) [[unlikely]]
            {
                ERR << "fatal: path" << quoted(filename) << "is a directory!";
                return false;
            }

            cv::Mat mat = cv::imread(cv::String{filename.data(), filename.size()}, cv::IMREAD_ANYCOLOR), mat2;
            if ((Group_)j == Group_::top)
            {
                cv::rotate(mat, mat2, cv::ROTATE_90_COUNTERCLOCKWISE);
                using std::swap;
                swap(mat, mat2);
            }

            const auto size = Vector2ui{(unsigned)mat.cols, (unsigned)mat.rows};

            if (size != expected_size) [[unlikely]]
            {
                ERR << "fatal: wrong image size, expected"
                    << resolution{expected_size} << colon(',')
                    << "actual" << resolution{size}
                    << "-- file" << filename;
                return false;
            }

            cv::Mat4b buf;
            if (!convert_to_bgra32(mat, buf)) [[unlikely]]
            {
                ERR << "fatal: unknown image pixel format:"
                    << "channels" << mat.channels() << colon(',')
                    << "depth" << cv::depthToString(mat.depth()) << colon(',')
                    << "type" << cv::typeToString(mat.type()) << colon(',')
                    << "for" << quoted(filename);
                return false;
            }

            st.frames.push_back({.size = size, .mat = std::move(buf)});
        }

        if (count == 0)
        {
            ERR << "fatal: no files found for" << quoted2(dir_name) << "/" << quoted2(group_name);
            return false;
        }

        fm_assert(start + count == st.frames.size());
        new_group.count = count;
        new_group.index = start;
    }
    else
    {
        new_group.count = 0;
        new_group.index = (uint32_t)-1;
    }

    return true;
}

bool do_direction(state& st, size_t i)
{
    const auto name = wall_atlas::directions[i].name;
    auto& atlas = st.new_atlas;
    DBG << "  direction" << quoted2(name);

    const auto& old_dir = get_direction(st.old_atlas, i);

    fm_assert(!atlas.direction_mask[i]);
    fm_assert(!atlas.direction_map[i]);
    const auto dir_idx = atlas.direction_array.size();
    fm_assert(dir_idx == (uint8_t)dir_idx);

    atlas.direction_mask[i] = 1;
    atlas.direction_map[i] = DirArrayIndex{(uint8_t)dir_idx};

    auto dir = Direction {
        .passability = old_dir.passability,
    };

    for (auto [_str, ptr, tag] : Direction::groups)
    {
        const auto& old_group = old_dir.*ptr;
        if (!old_group.is_defined)
            continue;
        if (!do_group(st, i, (size_t)tag, dir.*ptr))
            return false;
    }

    st.new_atlas.direction_array.push_back(std::move(dir));

    return true;
}

bool do_input_file(state& st)
{
    DBG << "input" << quoted(st.old_atlas.header.name) << colon(',') << quoted(st.opts.input_file);

    fm_assert(loader.check_atlas_name(st.old_atlas.header.name));
    fm_assert(st.old_atlas.direction_mask.any());

    auto& atlas = st.new_atlas;
    atlas.header = std::move(const_cast<wall_atlas_def&>(st.old_atlas).header);

    fm_assert(!atlas.frames.size());
    fm_assert(!atlas.direction_mask.any());
    fm_assert(atlas.direction_map == std::array<Wall::DirArrayIndex, Direction_COUNT>{});
    fm_assert(atlas.direction_array.empty());

    for (auto i = 0uz; i < Direction_COUNT; i++)
    {
        if (!st.old_atlas.direction_mask[i])
            continue;
        if (!do_direction(st, i))
            return false;
    }

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

struct argument_tuple
{
    options opts{};
    Arguments args{};
    bool ok = false;
};

argument_tuple parse_cmdline(int argc, const char* const* argv) noexcept
{
    Corrade::Utility::Arguments args{};
    args.addOption('o', "output"s, "\0"_s).setHelp("output"s, ""_s, "DIR"s);
    args.addArgument("input.json"s);
    args.parse(argc, argv);
    options opts;

    opts.input_file = Path::join(loader.startup_directory(), fixsep(args.value<StringView>("input.json")));
    opts.input_dir = Path::split(opts.input_file).first();
    if (auto val = args.value<StringView>("output"); val != "\0"_s)
        opts.output_dir = Path::join(loader.startup_directory(), fixsep(args.value<StringView>("output")));
    else
        opts.output_dir = opts.input_dir;

    if (opts.output_dir.isEmpty())
        opts.output_dir = opts.input_dir;

    //DBG << "input-dir" << opts.input_dir;
    //DBG << "output-dir" << opts.output_dir;

    if (!Path::exists(opts.input_file))
        Error{Error::Flag::NoSpace} << "fatal: input file '" << opts.input_file << "' doesn't exist";
    else if (!Path::isDirectory(opts.output_dir))
        Error{Error::Flag::NoSpace} << "fatal: output directory '" << opts.output_dir << "' doesn't exist";
    else if (Path::isDirectory(opts.input_file))
        Error{Error::Flag::NoSpace} << "fatal: input file '" << opts.input_file << "' is a directory";
    else
    {
        fm_assert(opts.output_dir);
        fm_assert(opts.input_file);
        fm_assert(opts.input_dir);
        return { .opts = std::move(opts), .args = std::move(args), .ok = true };
    }

    return {};
}

[[nodiscard]] int usage(const Arguments& args) noexcept
{
    Error{Error::Flag::NoNewlineAtTheEnd} << args.usage();
    return EX_USAGE;
}

} // namespace

} // namespace floormat::wall_tool

using namespace floormat;
using namespace floormat::wall_tool;

int main(int argc, char** argv)
{
    argv[0] = fix_argv0(argv[0]);
    auto [opts, args, opts_ok] = parse_cmdline(argc, argv);
    if (!opts_ok)
        return usage(args);

    auto old_atlas = wall_atlas_def::deserialize(opts.input_file);
    auto new_atlas = wall_atlas_def{};
    auto frames = std::vector<frame>{}; frames.reserve(64);
    auto error = EX_DATAERR;

    auto st = state {
        .opts = opts,
        .old_atlas = old_atlas,
        .new_atlas = new_atlas,
        .frames = frames,
        .error = error,
    };
    if (!do_input_file(st))
    {
        fm_assert(error);
        return error;
    }

    return 0;
}
