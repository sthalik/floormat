#include "app.hpp"
#include "src/nanosecond.inl"
#include "src/fps-counter.hpp"
#include <mg/Functions.h>

namespace floormat {

namespace {

constexpr auto ns_60hz = Ns{uint64_t{16666667}};
constexpr auto ns_30hz = Ns{uint64_t{33333333}};

// Tolerances sit 3-4x outside the error of a double-precision model of the filter, so that
// retuning fc_min or beta doesn't mean recomputing every number here.
constexpr float eps = 1e-6f;

float run(FPS_Counter& c, Ns dt, uint32_t frames)
{
    float ret = 0;
    for (auto i = 0u; i < frames; i++)
        ret = c.update(dt);
    return ret;
}

uint64_t lcg(uint64_t& state)
{
    state = (state*1103515245 + 12345) & 0x7fffffff;
    return state;
}

Ns jittered(uint64_t& state, double amplitude)
{
    const auto frac = double(lcg(state) % 4001) / 2000. - 1.;
    return Ns{uint64_t(double(ns_60hz.stamp) * (1 + amplitude*frac))};
}

void test_steady()
{
    FPS_Counter c;
    fm_assert(c.get() == 0);
    run(c, ns_60hz, 600);
    fm_assert(Math::abs(c.get() - 60.f) < 0.01f);
    fm_assert(Math::abs(c.get() - c.update(ns_60hz)) < eps);
}

void test_step_down()
{
    FPS_Counter c;
    run(c, ns_60hz, 120);
    fm_assert(Math::abs(c.get() - 60.f) < 0.01f);
    run(c, ns_30hz, 60);
    fm_assert(Math::abs(c.get() - 30.f) < 0.25f);
}

void test_step_up()
{
    FPS_Counter c;
    run(c, ns_30hz, 120);
    fm_assert(Math::abs(c.get() - 30.f) < 0.01f);
    run(c, ns_60hz, 180);
    fm_assert(Math::abs(c.get() - 60.f) < 0.5f);
}

// The gate is the relative deviation, not its derivative, so wall-clock reaction time must
// not track the frame rate. Dividing by dt made 1000 fps react 10x faster than 60.
void test_rate_independence()
{
    constexpr auto measure = [](uint32_t fps) {
        FPS_Counter c;
        const auto fast = Ns{uint64_t{1000000000} / fps};
        const auto slow = Ns{fast.stamp * 2};
        const auto target = (float)fps * 0.5f;
        run(c, fast, fps*3);
        for (auto i = 1u; i <= fps*20; i++)
        {
            c.update(slow);
            if (Math::abs(c.get() - target) <= target * 0.01f)
                return (float)i * (float)slow.stamp * 1e-9f;
        }
        return 1e9f;
    };

    const auto t60 = measure(60), t1000 = measure(1000);
    fm_assert(t60 > 0.3f && t60 < 2.f);
    fm_assert(Math::abs(t1000/t60 - 1.f) < 0.5f);
}

void test_jitter()
{
    FPS_Counter c;
    uint64_t state = 12345;
    float lo = 1e9f, hi = 0, sum = 0;
    uint32_t count = 0;

    for (auto i = 0u; i < 2000; i++)
    {
        c.update(jittered(state, 0.2));
        if (i >= 300)
        {
            const auto v = c.get();
            lo = Math::min(lo, v);
            hi = Math::max(hi, v);
            sum += v;
            count++;
        }
    }

    fm_assert(lo > 54 && hi < 64);
    // reads low under jitter: alpha grows with dt, so long frames get more weight in the
    // smoothed frame time. -0.05 fps at +-5% jitter, -0.20 at +-10%, -0.77 at +-20%.
    fm_assert(Math::abs(sum/(float)count - 60.f) < 1.5f);
}

void test_stall()
{
    FPS_Counter c;
    run(c, ns_60hz, 120);

    // A 200 ms frame is most of the filter's time constant, so it must show. Smoothing frame
    // time rather than fps makes the dip far deeper than the old filter's.
    c.update(200*Milliseconds);
    fm_assert(c.get() < 15);

    uint32_t frames = 0;
    while (c.get() < 59 && ++frames < 60)
        c.update(ns_60hz);
    fm_assert(frames <= 20);
}

void test_settle()
{
    FPS_Counter c{500*Milliseconds};
    fm_assert(c.get_settle_time() == 500*Milliseconds);
    fm_assert(run(c, ns_60hz, 29) == 0);
    fm_assert(c.get() == 0);
    fm_assert(Math::abs(c.update(ns_60hz) - 60.f) < 0.01f);

    c.reset();
    fm_assert(c.get() == 0);
    fm_assert(c.get_settle_time() == 500*Milliseconds);
    fm_assert(run(c, ns_60hz, 29) == 0);
    fm_assert(Math::abs(c.update(ns_60hz) - 60.f) < 0.01f);
}

void test_bogus_dt()
{
    FPS_Counter c;
    run(c, ns_60hz, 120);
    fm_assert(c.update(11*Seconds) == 0);
    fm_assert(c.get() == 0);

    run(c, ns_60hz, 120);
    fm_assert(Math::abs(c.get() - 60.f) < 0.01f);

    // 1 ns frames must neither divide by zero nor blow up into infinity
    FPS_Counter d;
    run(d, Ns{uint64_t{1}}, 100);
    fm_assert(d.get() > 0 && d.get() < 2e9f);
}

void test_copy()
{
    FPS_Counter a{500*Milliseconds};
    run(a, ns_60hz, 120);
    FPS_Counter b = a;
    fm_assert(Math::abs(a.get() - b.get()) < eps);
    fm_assert(b.get_settle_time() == a.get_settle_time());
    fm_assert(Math::abs(run(a, ns_30hz, 20) - run(b, ns_30hz, 20)) < eps);
}

} // namespace

void Test::test_fps()
{
    test_steady();
    test_step_down();
    test_step_up();
    test_rate_independence();
    test_jitter();
    test_stall();
    test_settle();
    test_bogus_dt();
    test_copy();
}

} // namespace floormat
