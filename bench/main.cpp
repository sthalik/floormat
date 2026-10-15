#include "loader/loader.hpp"
#include "compat/headless.hpp"
#include "compat/assert.hpp"
#include "src/hwy.hpp"
#include <cstdlib>
#include <cstring>
#include <hwy/targets.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

#define main bench_main
int bench_main(int argc, char** argv);
BENCHMARK_MAIN();
#undef main

struct bench_app final : private FM_APPLICATION
{
    using Application = FM_APPLICATION;
    explicit bench_app(int argc, char** argv);

    int exec() override;
    ~bench_app();

    int argc;
    char** argv;
};
bench_app::~bench_app() { loader.destroy(); }

int argc_ = 0; // NOLINT

bench_app::bench_app(int argc, char** argv) :
    Application {
        {argc_, nullptr},
        Configuration{}.setFlags(Configuration::Flag::QuietLog)
    },
    argc{argc}, argv{argv}
{
}

int bench_app::exec()
{
    if (const auto* s = getenv("FLOORMAT_HWY_TARGET"); s && *s)
    {
        int64_t t = 0;
        if (!strcmp(s, "avx2"))
            t = HWY_AVX2;
        else if (!strcmp(s, "sse4"))
            t = HWY_SSE4;
        else
            fm_abort("FLOORMAT_HWY_TARGET must be avx2 or sse4");
        hwy::SetSupportedTargetsForTest(t);
        vqsort_refresh_targets();
    }
    // Lower bits are preferred targets, so dispatch picks the lowest one compiled and supported.
    const auto avail = hwy::SupportedTargets() & HWY_TARGETS;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#endif
    benchmark::AddCustomContext("hwy_target", hwy::TargetName(avail & -avail));
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    return bench_main(argc, argv);
}

} // namespace

} // namespace floormat

using namespace floormat;

int main(int argc, char** argv)
{
    int status;
    {   auto app = bench_app{argc, argv};
        status = app.exec();
    }
    return status;
}
