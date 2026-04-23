#include "fps-counter.hpp"
#include "nanosecond.inl"
#include <cmath>

#include "compat/debug.hpp"

namespace floormat {

#if defined __GNUG__ && defined __x86_64__ && !defined __CLION_IDE__
static_assert(sizeof(long double) > sizeof(double));
#endif

struct FPS_Counter::Impl // do not disable pimpl, long double is unstable across the boundary
{
    long double fps = 0;
    Ns total_time;

    Ns settle_time;
    bool value_ok = false;
};

FPS_Counter::FPS_Counter(Ns settle_time) noexcept: p{new Impl}
{
    p->settle_time = settle_time;
}

FPS_Counter::~FPS_Counter() noexcept { delete p; }

FPS_Counter::FPS_Counter(const FPS_Counter& other) noexcept: p{new Impl}
{
    *p = *other.p;
}

FPS_Counter& FPS_Counter::operator=(const FPS_Counter& other) noexcept
{
    if (this != &other)
        *p = *other.p;
    return *this;
}

void FPS_Counter::reset()
{
    p->fps = 0;
    p->total_time = Ns{};
    p->value_ok = false;
}

float FPS_Counter::get() const
{
    return p->value_ok ? (float)p->fps : 0;
}

float FPS_Counter::update(Ns ns)
{
    auto& fps = p->fps;

    const long double dt = std::fmax(1e-9L, static_cast<long double>(ns.stamp) * 1e-9L);
    const long double fps_inst = 1 / dt;
    constexpr long double tau = 0.25L; // reaction time in seconds
    const long double alpha = 1 - std::exp(-dt / tau);

    if (ns > 10*Seconds) [[unlikely]]
    {
        reset();
        return 0;
    }
    else if ((p->total_time += ns) < p->settle_time) [[unlikely]]
    {
        fps = fps_inst;
        p->value_ok = false;
        return 0;
    }
    else
    {
        fps = fps * (1 - alpha) + fps_inst * alpha;
        p->value_ok = true;
    }

    return (float)fps + 0.02f; // XXX HACK: make "60 FPS" look more stable
}

void FPS_Counter::set_settle_time(Ns time) { p->settle_time = time; }
Ns FPS_Counter::get_settle_time() const { return p->settle_time; }

} // namespace floormat
