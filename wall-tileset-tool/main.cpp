#include "compat/assert.hpp"
#include "compat/sysexits.hpp"
#include "compat/fix-argv0.hpp"
#include "src/wall-atlas.hpp"
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

Triple<options, Arguments, bool> parse_cmdline(int argc, const char* const* argv) noexcept
{
    Corrade::Utility::Arguments args{};
    args.addOption('o', "output"s).setHelp("output"s, ""s, "DIR"s);
    args.addArgument("input.json"s);
    args.parse(argc, argv);
    options opts;
    //Path::exists(args.value<StringView>());

    opts.output_dir = Path::join(loader.startup_directory(), args.value<StringView>("output"));
    opts.input_file = Path::join(loader.startup_directory(), args.value<StringView>("input.json"));
    opts.input_dir = Path::split(opts.input_file).first();

    if (opts.output_dir.isEmpty())
        opts.output_dir = opts.input_dir;

    return { std::move(opts), std::move(args), false };
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
