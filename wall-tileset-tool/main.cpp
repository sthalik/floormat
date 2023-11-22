#include "compat/assert.hpp"
#include "compat/sysexits.hpp"
#include "compat/fix-argv0.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
#include <utility>
#include <tuple>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/TripleStl.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/Arguments.h>
#include <nlohmann/json.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

namespace floormat {

using Corrade::Utility::Arguments;
using namespace std::string_literals;

namespace {

struct options
{
    String input_dir, input_file, output_dir;
};

std::shared_ptr<wall_atlas> read_from_file(StringView filename)
{
    using namespace floormat::Wall::detail;

    const auto jroot = json_helper::from_json_(filename);
    auto header = read_info_header(jroot);
    if (!loader.check_atlas_name(header.name))
        fm_abort("bad atlas name '%s'!", header.name.data());

    return {};
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
    args.addOption('o', "output"s).setHelp("output"s, ""s, "DIR"s);
    args.addArgument("input.json"s);
    args.parse(argc, argv);
    options opts;

    opts.output_dir = Path::join(loader.startup_directory(), fixsep(args.value<StringView>("output")));
    opts.input_file = Path::join(loader.startup_directory(), fixsep(args.value<StringView>("input.json")));
    opts.input_dir = Path::split(opts.input_file).first();

    if (opts.output_dir.isEmpty())
        opts.output_dir = opts.input_dir;

    if (!Path::exists(opts.input_file))
        Error{Error::Flag::NoSpace} << "fatal: input file '" << opts.input_file << "' doesn't exist";
    else if (!Path::isDirectory(opts.output_dir))
        Error{Error::Flag::NoSpace} << "fatal: output directory '" << opts.output_dir << "' doesn't exist";
    else if (Path::isDirectory(opts.input_file))
        Error{Error::Flag::NoSpace} << "fatal: input file '" << opts.input_file << "' is a directory";
    else
        return { std::move(opts), std::move(args), true };

    return {};
}

[[nodiscard]] static int usage(const Arguments& args) noexcept
{
    Error{Error::Flag::NoNewlineAtTheEnd} << args.usage();
    return EX_USAGE;
}

} // namespace

} // namespace floormat

using namespace floormat;

int main(int argc, char** argv)
{
    argv[0] = fix_argv0(argv[0]);
    auto [opts, args, opts_ok] = parse_cmdline(argc, argv);

    return 0;
}
