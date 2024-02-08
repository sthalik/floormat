#include "main.hpp"
#include "compat/assert.hpp"
#include "compat/sysexits.hpp"
#include "compat/fix-argv0.hpp"
#include "compat/format.hpp"
#include "compat/debug.hpp"
#include "src/tile-constants.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "loader/loader.hpp"
#include <utility>
#include <tuple>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringIterable.h>
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

    switch (src.type())
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

bool save_image(state st)
{
    fm_assert(st.new_atlas.frames.isEmpty());
    arrayReserve(st.new_atlas.frames, 64);

    uint32_t max_height = 0;
    for (const auto& group : st.groups)
    {
        const auto expected_size = wall_atlas::expected_size(st.new_atlas.header.depth, group.G);
        max_height = std::max(max_height, expected_size.y());
    }
    fm_assert(max_height > 0);
    fm_assert(max_height == (uint32_t)iTILE_SIZE.z()); // todo?

    uint32_t xpos = 0;
    Vector2ui max;
    for (auto& group : st.groups)
    {
        const auto size = wall_atlas::expected_size(st.new_atlas.header.depth, group.G);
        const auto width = size.x(), height = size.y();
        uint32_t ypos = 0;
        bool started = false;
        for (auto& frame : group.frames)
        {
            frame.offset = {xpos, ypos};
            max = Math::max(max, frame.offset + size);
            started = true;
            fm_assert(max <= Vector2ui{(unsigned)max_image_dimension});
            fm_assert(frame.size == size);
            if (ypos + height*2 <= max_height)
                ypos += height;
            else
            {
                ypos = 0;
                xpos += width;
                started = false;
            }
        }
        if (started)
            xpos += width;
    }
    fm_assert(max.product() > 0);

    st.dest.create((int)max.y(), (int)max.x());
    st.dest.setTo(cv::Scalar{255, 0, 255, 0});

    for (const auto& group : st.groups)
    {
        for (const auto& frame : group.frames)
        {
            //Debug{} << "g" << (int)group.G << frame.offset << frame.size;
            auto rect = cv::Rect{(int)frame.offset.x(), (int)frame.offset.y(),
                                 (int)frame.size.x(), (int)frame.size.y()};
            frame.mat.copyTo(st.dest(rect));
            arrayAppend(st.new_atlas.frames, Wall::Frame {
                .offset = frame.offset, .size = frame.size
            });
        }
    }

    auto filename = ""_s.join({Path::join(st.opts.output_dir, st.new_atlas.header.name), ".png"_s});
    if (!st.opts.use_alpha)
    {
        cv::Mat3b img;
        cv::cvtColor(st.dest, img, cv::COLOR_BGRA2BGR);
        cv::imwrite(filename, img);
    }
    else
        cv::imwrite(filename, st.dest);
    return true;
}

bool save_json(state st)
{
    using namespace floormat::Wall::detail;
    fm_assert(!st.new_atlas.frames.isEmpty());
    fm_assert(st.new_atlas.header.depth > 0);
    auto filename = ""_s.join({Path::join(st.opts.output_dir, st.new_atlas.header.name), ".json"_s});
    st.new_atlas.serialize(filename);
    return true;
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
    fm_assert((size_t)st.groups.at(j).G == j);

    auto& frames = [&]() -> auto&& {
        for (auto& g : st.groups)
            if ((size_t)g.G == j)
                return g.frames;
        fm_abort("can't find ground '%d'", (int)j);
    }();

    uint32_t count = 0, start = st.n_frames;
    new_group = old_group;
    new_group.is_defined = true;
    new_group.pixel_size = Vector2ui(expected_size);

    for (;;)
    {
        auto filename = asformat("{}/{:04}.png"_cf, path, count+1);
        if (!Path::exists(filename))
            break;
        if (Path::isDirectory(filename)) [[unlikely]]
        {
            ERR << "fatal: path" << quoted(filename) << "is a directory!";
            return false;
        }

        count++;
        st.n_frames++;

        cv::Mat mat = cv::imread(filename, cv::IMREAD_ANYCOLOR), mat2;
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
        if (mat.channels() == 4)
            st.opts.use_alpha = true;
        if (!convert_to_bgra32(mat, buf)) [[unlikely]]
        {
            ERR << "fatal: unknown image pixel format:"
                << "channels" << mat.channels() << colon(',')
                << "depth" << cv::depthToString(mat.depth()) << colon(',')
                << "type" << cv::typeToString(mat.type()) << colon(',')
                << "for" << quoted(filename);
            return false;
        }

        frames.push_back({
            .mat = std::move(buf),
            .size = {(unsigned)mat.cols, (unsigned)mat.rows},
        });
    }

    if (count == 0)
    {
        ERR << "fatal: no files found for" << quoted2(dir_name) << "/" << quoted2(group_name);
        return false;
    }

    DBG << "      " << Debug::nospace << count << (count == 1 ? "frame" : "frames");

    fm_assert(start + count == st.n_frames);
    new_group.count = count;
    new_group.index = start;

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

    auto dir = Direction{};

    for (auto [_str, ptr, tag] : Direction::groups)
    {
        const auto& old_group = old_dir.*ptr;
        if (!old_group.is_defined)
            continue;
        if (!do_group(st, i, (size_t)tag, dir.*ptr))
            return false;
    }

    arrayAppend(st.new_atlas.direction_array, std::move(dir));

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
    fm_assert(atlas.direction_array.isEmpty());
    arrayReserve(atlas.direction_array, Direction_COUNT);

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
    auto dest = cv::Mat4b{};
    auto error = EX_DATAERR;
    uint32_t n_frames = 0;

    auto groups = std::vector<group>{};
    groups.reserve(Wall::Group_COUNT);

    for (auto [name, ptr, val] : Wall::Direction::groups)
    {
        groups.push_back({
            .frames = std::vector<frame>{},
            .G = val,
        });
    }

    auto st = state {
        .opts = opts,
        .old_atlas = old_atlas,
        .new_atlas = new_atlas,
        .groups = groups,
        .n_frames = n_frames,
        .dest = dest,
        .error = error,
    };
    if (!do_input_file(st) || !save_image(st) || !save_json(st))
    {
        fm_assert(error);
        return error;
    }

    return 0;
}
