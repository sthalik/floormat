#include "app.hpp"
#include "loader/loader.hpp"
#include <stdlib.h> // NOLINT(*-deprecated-headers)
#include <cstdio>
#include <Corrade/Containers/StringView.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Timeline.h>
#include <Magnum/GL/Context.h>

namespace floormat {

namespace {

bool is_log_quiet() // copy-pasted from src/chunk.cpp
{
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::QuietLog);
}

} // namespace

test_app::test_app(const Arguments& arguments):
      Application {
          arguments,
          Configuration{}
      }
{
}

test_app::~test_app()
{
    loader.destroy();
}

int test_app::exec()
{
    constexpr auto SV_flags = StringViewFlag::Global|StringViewFlag::NullTerminated;
    constexpr auto name_prefix = "test_"_s;

#define FM_TEST(name) { ( StringView{#name, arraySize(#name)-1, SV_flags} ), ( &(name) ), }

    constexpr struct {
        StringView name;
        void(*function)();
    } list[] = {
        FM_TEST(test_coords),
        FM_TEST(test_tile_iter),
        FM_TEST(test_magnum_math),
        FM_TEST(test_math),
        FM_TEST(test_entity),
        FM_TEST(test_iptr),
        FM_TEST(test_hash),
        FM_TEST(test_raycast),
        FM_TEST(test_json),
        FM_TEST(test_wall_atlas),
        FM_TEST(test_json2),
        FM_TEST(test_wall_atlas2),
        FM_TEST(test_json3),
        FM_TEST(test_bitmask),
        FM_TEST(test_loader),
        FM_TEST(test_serializer1),
        FM_TEST(test_scenery),
        FM_TEST(test_astar_pool),
        FM_TEST(test_astar),
        FM_TEST(test_dijkstra), // todo add dummy atlases to avoid expensive loading
        FM_TEST(test_load_all),
        FM_TEST(test_zzz_misc),
    };

    if (is_log_quiet())
        for (const auto [_, fun] : list)
            (*fun)();
    else
    {
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
        for (const auto [name, _] : list)
            max_tabs = Math::max(max_tabs, get_tabs(name));
        max_tabs++;
        if (max_tabs > tab_limit)
            max_tabs = 1;

        std::fflush(s);

        for (auto [name, fun] : list)
        {
            name = name.exceptPrefix(name_prefix);
            std::fwrite(name.data(), name.size(), 1, s);
            std::fflush(stdout);
            auto ms = get_time(fun);
            std::fwrite(sep.data(), sep.size(), 1, s);
            auto num_tabs = max_tabs - get_tabs(name);
            fm_assert(num_tabs <= tab_limit);
            for (auto i = 0uz; i < num_tabs; i++)
                std::fputc('\t', s);
            std::fprintf(s, "% 9.3f ms\n", (double)ms);
            std::fflush(s);
        }
    }

    return 0;
}

} // namespace floormat

int main(int argc, char** argv)
{
#ifdef _WIN32
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    if (const auto* s = std::getenv("MAGNUM_LOG"); !s || !*s)
        ::_putenv("MAGNUM_LOG=quiet");
#else
        ::setenv("MAGNUM_LOG", "quiet", 0);
#endif
    floormat::test_app application{{argc, argv}};
    return application.exec();
}
