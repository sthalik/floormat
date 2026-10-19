#include "app.hpp"
#include "loader/loader.hpp"
#include "compat/array-size.hpp"
#include "compat/headless.hpp"
#include "compat/sysexits.hpp"
#include <stdlib.h> // NOLINT(*-deprecated-headers)
#include <cstdio>
#include <cr/StringView.h>
#include <cr/Arguments.h>
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

struct options
{
    uint32_t repeat;
    bool no_gl;
};

} // namespace

bool has_gl_context() { return gl_context; }

struct App final : private FM_APPLICATION
{
    using Application = FM_APPLICATION;
    explicit App(const Arguments& arguments, const options& opts);
    ~App();

    int exec() override;

private:
    uint32_t repeat;
};

App::App(const Arguments& arguments, const options& opts):
      Application{arguments, NoCreate},
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

struct test_entry
{
    StringView name;
    void(*function)();
    bool needs_gl;
};

void run_tests(ArrayView<const test_entry> list, bool quiet)
{
    constexpr auto name_prefix = "test_"_s;

    if (quiet)
    {
        for (const auto [_, fun, needs_gl] : list)
            if (!needs_gl || has_gl_context())
                (*fun)();
        return;
    }

    FILE* const s = stdout;
    static constexpr auto sep = ""_s;
    constexpr auto get_tabs = [](StringView name) constexpr {
        return (name.size()+sep.size()) / 8;
    };
    constexpr size_t tab_limit = 5;
    constexpr auto get_time = [](auto&& fn) {
        Timeline t;
        t.start();
        (*fn)();
        return t.currentFrameTime() * 1e3f;
    };

    size_t max_tabs = 1;
    for (const auto& e : list)
        max_tabs = Math::max(max_tabs, get_tabs(e.name));
    max_tabs++;
    if (max_tabs > tab_limit)
        max_tabs = 1;

    std::fflush(s);

    for (auto [name, fun, needs_gl] : list)
    {
        name = name.exceptPrefix(name_prefix);
        std::fwrite(name.data(), name.size(), 1, s);
        if constexpr(!sep.isEmpty())
            std::fwrite(sep.data(), sep.size(), 1, s);
        auto num_tabs = max_tabs - get_tabs(name) - 1;
        std::fputc('\t', s);
        std::fflush(stdout);
        const bool skip = needs_gl && !has_gl_context();
        auto ms = skip ? 0.f : get_time(fun);
        fm_assert(num_tabs <= tab_limit);
        for (auto i = 0uz; i < num_tabs; i++)
            std::fputc('\t', s);
        if (skip)
            std::fprintf(s, "%12s\n", "skipped");
        else
            std::fprintf(s, "%12.3f ms\n", (double)ms);
        std::fflush(s);
    }
}

} // namespace

int App::exec()
{
    constexpr auto SV_flags = StringViewFlag::Global|StringViewFlag::NullTerminated;

#define FM_TEST_(name, gl) { ( StringView{#name, array_size(#name)-1, SV_flags} ), ( &(name) ), (gl), }
#define FM_TEST(name) FM_TEST_(name, false)
#define FM_TEST_GL(name) FM_TEST_(name, true)

    constexpr test_entry list[] = {
        FM_TEST(test_local),
        // fast
        FM_TEST(test_magnum_math),
        FM_TEST(test_util),
        FM_TEST(test_split),
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
        FM_TEST_GL(test_slide),
        FM_TEST_GL(test_corridor),
        FM_TEST_GL(test_dijkstra),
        FM_TEST_GL(test_dijkstra_reference),
        FM_TEST_GL(test_cover_point),
        FM_TEST_GL(test_loader2),
        FM_TEST_GL(test_loader3),
        FM_TEST_GL(test_saves),
        FM_TEST_GL(test_sprites),
        FM_TEST(test_multi_level_table),
    };

#undef FM_TEST_GL
#undef FM_TEST
#undef FM_TEST_

    const bool quiet = is_log_quiet();

    for (auto i = 0u; i < repeat; i++)
    {
        if (repeat > 1)
        {
            std::printf("=== iteration %u/%u\n", i+1, repeat);
            std::fflush(stdout);
        }
        run_tests(list, quiet);
    }

    return 0;
}

namespace {

options parse_cmdline(int argc, char** argv)
{
    Corrade::Utility::Arguments args{};
    args.addSkippedPrefix("magnum")
        .addOption("repeat", "1").setHelp("repeat", "run the whole test suite N times", "N")
        .addBooleanOption("no-gl").setHelp("no-gl", "don't create a GL context, skip the tests that need one")
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
    return { value, args.isSet("no-gl") };
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
    const auto opts = floormat::Test::parse_cmdline(argc, argv);
    auto app = floormat::Test::App{{argc, argv}, opts};
    return app.exec();
}
