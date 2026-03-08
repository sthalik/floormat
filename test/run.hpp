#pragma once
#include "compat/function2.fwd.hpp"
#include "src/rotation.hpp"
#include "src/nanosecond.hpp"
#include "src/log.hpp"
#include "src/critter.hpp"

namespace floormat::Run {

using enum rotation;
using fu2::function_view;
using Function = function_view<Ns() const>;

critter_proto make_proto(float accel);
void mark_all_modified(world& w);

struct Start
{
    StringView name, instance;
    point pt;
    double accel = 1;
    enum rotation rotation = N;
    bool silent = false;
#if 1
    bool quiet = !is_log_verbose();
    bool verbose = false;
#elif 0
    bool verbose = true;
    bool quiet = false;
#elif 1
    bool verbose = false;
    bool quiet = false;
#elif 0
    bool quiet = is_log_quiet();
    bool verbose = is_log_standard() || is_log_verbose();
#else
    bool quiet = is_log_quiet() || is_log_standard();
    bool verbose = is_log_verbose();
#endif
};

struct Expected
{
    point pt;
    Ns time;
};

struct Grace
{
    Ns time = Ns{250};
    uint32_t distance_L2 = 24;
    uint32_t max_steps = 1'000;
    bool no_crash = false;

    static constexpr uint32_t very_slow_max_steps = 120'000,
                              slow_max_steps      = 12'000,
                              default_max_steps   = 1'200;
};

bool run(world& w, const function_view<Ns() const>& make_dt,
         Start start, Expected expected, Grace grace = {});

#ifndef __CLION_IDE__
constexpr auto constantly(const auto& x) noexcept {
    return [x]<typename... Ts> (const Ts&...) constexpr -> const auto& { return x; };
}
#else
constexpr auto constantly(Ns x) noexcept { return [x] { return x; }; }
#endif

} // namespace floormat::Run
