#include "app.hpp"
#include "loader/loader.hpp"
#include "compat/array-size.hpp"
#include "compat/headless.hpp"
#include "compat/sysexits.hpp"
#include <stdlib.h> // NOLINT(*-deprecated-headers)
#include <cstdio>
#include <cr/Arguments.h>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Timeline.h>
#include <mg/Context.h>

namespace floormat::Test {

namespace {

bool gl_context = false;

bool is_log_quiet() // copy-pasted from src/chunk.cpp
{
    if (!gl_context)
        return false;
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::QuietLog);
}

enum class gl_use : uint8_t { none, optional, required, };

struct test_entry
{
    StringView name;
    void(*function)();
    gl_use gl;
};

constexpr auto SV_flags = StringViewFlag::Global|StringViewFlag::NullTerminated;

#define FM_TEST_(name, gl) { ( StringView{#name, array_size(#name)-1, SV_flags} ), ( &(name) ), (gl), }
#define FM_TEST(name) FM_TEST_(name, gl_use::none)
#define FM_TEST_GL(name) FM_TEST_(name, gl_use::required)
#define FM_TEST_GL_OPTIONAL(name) FM_TEST_(name, gl_use::optional)

constexpr test_entry test_list[] = {
    FM_TEST(test_local),
    // fast
    FM_TEST(test_magnum_math),
    FM_TEST(test_util),
    FM_TEST(test_split),
    FM_TEST(test_getopt),
    FM_TEST(test_enumerate),
    FM_TEST(test_zip),
    FM_TEST(test_math),
    FM_TEST(test_rtree_pool),
    FM_TEST(test_astar_pool),
    FM_TEST(test_pool_registry),
    FM_TEST(test_coords),
    FM_TEST(test_intra_coord),
    FM_TEST(test_crc64),
    FM_TEST(test_random),
    FM_TEST(test_bptr),
    FM_TEST(test_bptr_intrusive),
    FM_TEST_GL(test_chunk_iter),
    FM_TEST(test_entity),
    FM_TEST(test_float),
    FM_TEST(test_format),
    FM_TEST(test_fps),
    FM_TEST(test_texcoords),
    FM_TEST_GL(test_shader),
    FM_TEST(test_vqsort),
    // normal

    FM_TEST(test_bitmask),
    FM_TEST(test_json),
    FM_TEST_GL(test_json2),
    FM_TEST_GL(test_json3),
    FM_TEST_GL(test_loader),
    FM_TEST_GL(test_scenery),
    FM_TEST_GL(test_emit_quick),
    FM_TEST(test_raycast_line),
    FM_TEST_GL(test_raycast),
    FM_TEST_GL(test_passability_bbox),
    FM_TEST(test_hash),
    FM_TEST(test_path),
    FM_TEST(test_wall_atlas),
    FM_TEST_GL(test_wall_atlas2),
    // the rest are slow
    FM_TEST_GL(test_grid),
    FM_TEST_GL(test_rtree),
    FM_TEST_GL(test_astar),
    FM_TEST_GL(test_hole),
    FM_TEST_GL(test_save),
    FM_TEST(test_spinlock),
    FM_TEST(test_atomic),
    FM_TEST(test_atomic_wait),
    FM_TEST(test_chase_lev_deque),
    FM_TEST(test_event_count),
    FM_TEST(test_inplace_function),
    FM_TEST_GL(test_sprite_atlas),
    FM_TEST(test_spritebatch),
    FM_TEST_GL(test_critter),
    FM_TEST_GL(test_sweep_aabb),
    FM_TEST_GL(test_critter_walk),
    FM_TEST_GL(test_slide),
    FM_TEST_GL(test_corridor),
    FM_TEST_GL(test_dijkstra),
    FM_TEST_GL(test_dijkstra_reference),
    FM_TEST_GL(test_cover_point),
    FM_TEST_GL(test_loader2),
    FM_TEST_GL(test_loader3),
    FM_TEST_GL(test_saves),
    FM_TEST_GL(test_sprites),
    FM_TEST_GL_OPTIONAL(test_multi_level_table),
};

#undef FM_TEST_GL_OPTIONAL
#undef FM_TEST_GL
#undef FM_TEST
#undef FM_TEST_

struct options
{
    uint32_t repeat;
    bool no_gl;
    Array<test_entry> tests;
};

} // namespace

bool has_gl_context() { return gl_context; }

struct App final : private FM_APPLICATION
{
    using Application = FM_APPLICATION;
    explicit App(const Arguments& arguments, options&& opts);
    ~App();

    int exec() override;

private:
    Array<test_entry> tests;
    uint32_t repeat;
};

App::App(const Arguments& arguments, options&& opts):
      Application{arguments, NoCreate},
      tests{move(opts.tests)},
      repeat{opts.repeat}
{
    if (!opts.no_gl)
    {
        createContext();
        gl_context = true;
    }
}

App::~App()
{
    loader.destroy();
}

namespace {

void run_tests(ArrayView<const test_entry> list, bool quiet)
{
    if (quiet)
    {
        for (const auto [_, fun, gl] : list)
            if (gl != gl_use::required || has_gl_context())
                (*fun)();
        return;
    }

    FILE* const s = stdout;
    constexpr auto get_time = [](auto&& fn) {
        Timeline t;
        t.start();
        (*fn)();
        return t.currentFrameTime() * 1e3f;
    };

    unsigned longest_name = 0;
    for (auto e : list)
        longest_name = Math::max(longest_name, (unsigned)e.name.size());

    std::fflush(s);

    for (auto [name, fun, gl] : list)
    {
        name = name.exceptPrefix("test_"_s);
        std::fwrite(name.data(), name.size(), 1, s);

        const auto padding = longest_name + 1 - (unsigned)name.size();
        for (auto i = 0u; i < padding; i++)
            std::fputc(' ', s);
        std::fflush(s);

        const bool skip = gl == gl_use::required && !has_gl_context();
        auto ms = skip ? 0.f : get_time(fun);
        if (skip)
            std::fprintf(s, "%12s\n", "-");
        else
            std::fprintf(s, "%9.1f ms\n", (double)ms);
        std::fflush(s);
    }
}

} // namespace

int App::exec()
{
    const bool quiet = is_log_quiet();

    for (auto i = 0u; i < repeat; i++)
    {
        if (repeat > 1)
        {
            std::printf("=== iteration %u/%u\n", i+1, repeat);
            std::fflush(stdout);
        }
        run_tests(tests, quiet);
    }

    return 0;
}

namespace {

bool name_matches(StringView name, StringView arg)
{
    name = name.exceptPrefix("test_"_s);
    if (name.size() != arg.size())
        return false;
    for (auto i = 0u; i < (uint32_t)name.size(); i++)
        if (arg[i] != name[i] && !(arg[i] == '-' && name[i] == '_'))
            return false;
    return true;
}

Array<test_entry> select_tests(const Corrade::Utility::Arguments& args)
{
    constexpr auto list_size = (uint32_t)array_size(test_list);
    Array<test_entry> tests;
    arrayReserve(tests, list_size);
    bool seen[list_size] = {};
    const auto count = (uint32_t)args.arrayValueCount("tests");
    bool all = count == 0;

    for (auto i = 0u; i < count; i++)
    {
        const auto arg = args.arrayValue<StringView>("tests", i);
        if (arg == "all"_s)
        {
            all = true;
            continue;
        }
        auto j = 0u;
        while (j < list_size && !name_matches(test_list[j].name, arg))
            j++;
        if (j == list_size)
        {
            ERR_nospace << "unknown test '" << arg << "'";
            std::exit(EX_USAGE);
        }
        if (!seen[j])
        {
            seen[j] = true;
            arrayAppend(tests, test_list[j]);
        }
    }

    if (all)
    {
        arrayClear(tests);
        arrayAppend(tests, ArrayView<const test_entry>{test_list});
    }
    return tests;
}

options parse_cmdline(int argc, char** argv)
{
    using Corrade::Utility::Arguments;
    Arguments args{};
    args.addSkippedPrefix("magnum")
        .addArrayArgument("tests").setHelp("tests", "tests to run, in this order; none or 'all' runs every test", "NAME")
        .addOption("repeat", "1").setHelp("repeat", "run the tests N times", "N")
        .addBooleanOption("no-gl").setHelp("no-gl", "don't create a GL context, skip the tests that need one")
        .addBooleanOption('l', "list-tests").setHelp("list-tests", "print the tests that would run, then exit")
        // addArrayArgument() requires at least one value. Zero names runs every test.
        .setParseErrorCallback([](const Arguments&, Arguments::ParseError error, const std::string& key) {
            return error == Arguments::ParseError::MissingArgument && key == "tests";
        })
        .parse(argc, argv);
    const auto str = args.value<StringView>("repeat");
    uint32_t value = 0;
    int n = 0;
    // same digit guard as editor/app.cpp parse_uint
    if (str.isEmpty() || str[0] < '0' || str[0] > '9' ||
        std::sscanf(str.data(), "%u%n", &value, &n) != 1 || (size_t)n != str.size() || value == 0)
    {
        ERR_nospace << "invalid --repeat argument '" << str << "': should be a positive number";
        std::exit(EX_USAGE);
    }
    auto tests = select_tests(args);
    if (args.isSet("list-tests"))
    {
        for (const auto& e : tests)
            std::printf("%s\n", e.name.exceptPrefix("test_"_s).data());
        std::exit(0);
    }
    bool uses_gl = false;
    for (const auto& e : tests)
        uses_gl |= e.gl != gl_use::none;
    return { value, args.isSet("no-gl") || !uses_gl, move(tests) };
}

} // namespace

} // namespace floormat::Test

int main(int argc, char** argv)
{
    if (const auto* s = std::getenv("MAGNUM_LOG"); !s || !*s)
    {
#ifdef _WIN32
        ::_putenv("MAGNUM_LOG=default");
#else
        ::setenv("MAGNUM_LOG", "default", 0);
#endif
    }
    auto opts = floormat::Test::parse_cmdline(argc, argv);
    auto app = floormat::Test::App{{argc, argv}, floormat::move(opts)};
    return app.exec();
}
