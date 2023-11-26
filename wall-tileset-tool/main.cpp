#include "main.hpp"
#include "compat/assert.hpp"
#include "compat/sysexits.hpp"
#include "compat/fix-argv0.hpp"
//#include "compat/format.hpp"
#include "compat/debug.hpp"
#include "src/wall-atlas.hpp"
//#include "serialize/wall-atlas.hpp"
//#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
#include <utility>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/String.h>
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

Vector2i get_buffer_size(const wall_atlas_def& a)
{
    Vector2i size;

    for (auto i = 0uz; i < Direction_COUNT; i++)
    {
        auto idx = a.direction_map[i];
        if (!idx)
            continue;
        const auto& dir = a.direction_array[idx.val];
        for (auto j = 0uz; j < (size_t)Group_::COUNT; j++)
        {
            const auto& group = (dir.*(Direction::groups[j].member));
            if (!group.is_defined)
                continue;
            auto val = wall_atlas::expected_size(a.header.depth, (Group_)j);
            size = Math::max(size, val);
        }
    }

    if (!(size > Vector2i{0}))
        fm_abort("fatal: atlas '%s' has no defined groups", a.header.name.data());

    return size;
}

const Direction& get_direction(const wall_atlas_def& atlas, size_t i)
{
    fm_assert(atlas.direction_mask[i]);
    auto idx = atlas.direction_map[i];
    fm_assert(idx);
    fm_assert(idx < atlas.direction_array.size());
    return atlas.direction_array[idx.val];
}

Direction& get_direction(wall_atlas_def& atlas, size_t i)
{
    return const_cast<Direction&>(get_direction(const_cast<const wall_atlas_def&>(atlas), i));
}

bool do_group(state st, size_t i, size_t j, Group& new_dir)
{
    const wall_atlas_def& old_atlas = st.old_atlas;
    wall_atlas_def& new_atlas = st.new_atlas;
    const auto& old_dir = get_direction(old_atlas, (size_t)i);
    //auto& new_dir = get_direction(new_atlas, (size_t)i);

    DBG << "    group" << quoted2(Direction::groups[j].str);

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
        if (!do_group(st, i, (size_t)tag, dir.*ptr))
            return false;
    }

    st.new_atlas.direction_array.push_back(std::move(dir));

    return true;
}

bool do_input_file(state& st)
{
    DBG << "input" << quoted(st.old_atlas.header.name) << colon(',') << quoted(st.opts.input_file);

    fm_assert(!st.buffer.empty());
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

Triple<options, Arguments, bool> parse_cmdline(int argc, const char* const* argv) noexcept
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
        return { std::move(opts), std::move(args), true };
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
    auto s = "foo"_s;
    static_assert(std::is_same_v<decltype(quoted(StringView{"foo"}))::type, StringView>);
    static_assert(std::is_same_v<decltype(quoted(String{"foo"}))::type, String>);
    static_assert(std::is_same_v<decltype(quoted(s))::type, const StringView&>);

    argv[0] = fix_argv0(argv[0]);
    auto [opts, args, opts_ok] = parse_cmdline(argc, argv);
    if (!opts_ok)
        return usage(args);

    auto a = wall_atlas_def::deserialize(opts.input_file);
    auto buf_size = get_buffer_size(a);
    auto mat = cv::Mat4b{cv::Size{buf_size.x(), buf_size.y()}};
    auto new_atlas = wall_atlas_def{};
    auto error = EX_DATAERR;

    auto st = state {
        .opts = opts,
        .buffer = mat,
        .old_atlas = a,
        .new_atlas = new_atlas,
        .error = error,
    };
    if (!do_input_file(st))
    {
        fm_assert(error);
        return error;
    }

    return 0;
}
