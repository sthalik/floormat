#include "loader/loader.hpp"
#include "compat/headless.hpp"
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
bench_app::~bench_app() { loader_::destroy(); }

int argc_ = 0; // NOLINT

bench_app::bench_app(int argc, char** argv) :
    Application {
        {argc_, nullptr},
        Configuration{}
    },
    argc{argc}, argv{argv}
{
}

int bench_app::exec()
{
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
    loader_::destroy();
    return status;
}
