#include "big-atlas.hpp"
#include "tile/serialize.hpp"
#include <tuple>
#include <filesystem>
#include <Corrade/Utility/Arguments.h>

using Corrade::Utility::Arguments;

struct options final {
    std::filesystem::path input_dir, output_file;
};

static std::tuple<options, Arguments, bool> parse_cmdline(int argc, const char* const* argv) noexcept
{
    Corrade::Utility::Arguments args{};
    args.addOption('o', "output")
        .addArrayArgument("input");
    args.parse(argc, argv);
    options opts;
    opts.input_dir = args.value<std::string>("input");

    if (opts.input_dir.empty())
        opts.output_file = opts.input_dir.parent_path() / "big-atlas.json";

    return { std::move(opts), std::move(args), true };
}

int main(int argc, char** argv)
{
    big_atlas_builder builder;
    builder.add_atlas("images/metal1.png");
    builder.add_atlas("images/metal2.png");
    return 0;
}
